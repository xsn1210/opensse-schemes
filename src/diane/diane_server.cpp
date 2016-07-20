//
// Sophos - Forward Private Searchable Encryption
// Copyright (C) 2016 Raphael Bost
//
// This file is part of Sophos.
//
// Sophos is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// Sophos is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Sophos.  If not, see <http://www.gnu.org/licenses/>.
//


#include "diane_server.hpp"

#include "thread_pool.hpp"

#include <sse/crypto/block_hash.hpp>

namespace sse {
    namespace diane {
        
        DianeServer::DianeServer(const std::string& db_path) :
        edb_(db_path)
        {
        }

        DianeServer::DianeServer(const std::string& db_path, const size_t tm_setup_size) :
        edb_(db_path)
        {
            
        }

        std::list<index_type> DianeServer::search(const SearchRequest& req)
        {
            std::list<index_type> results;

            auto callback = [&results](index_type i)
            {
                results.push_back(i);
            };
            
            search(req, callback);
            
            return results;
        }

        void DianeServer::search(const SearchRequest& req, std::function<void(index_type)> post_callback)
        {
            index_type r;
            
            logger::log(logger::DBG) << "Expected matches: " << req.add_count << std::endl;
            logger::log(logger::DBG) << "Number of search nodes: " << req.token_list.size() << std::endl;

            auto derivation_prf = crypto::Prf<kUpdateTokenSize>(&req.kw_token);
            
            for (auto it_token = req.token_list.begin(); it_token != req.token_list.end(); ++it_token) {
                
                logger::log(logger::DBG) << "Search token key: " << hex_string(it_token->first) << std::endl;
                logger::log(logger::DBG) << "Search token depth: " << std::dec << (uint32_t)(it_token->second) << std::endl;
                
                // for now we implement the search algorithm in a naive way:
                // the tokens are iteratively generated using the derive_node function
                // this is not smart as some inner nodes will be recomputed several times.
                // we leave optimizations for later
                
                
                uint64_t count = 1 << it_token->second;
                
                for (uint64_t i = 0; i < count; i++) {
                    auto t = TokenTree::derive_node(it_token->first, i, it_token->second);
                    
                    logger::log(logger::DBG) << "Derived leaf token: " << hex_string(t) << std::endl;
                    
                    update_token_type ut;
                    std::array<uint8_t, sizeof(index_type)> mask;
                    
                    // derive the two parts of the leaf search token
                    // it avoids having to use some different IVs to have two different hash functions.
                    // it might decrease the security bounds by a few bits, but, meh ...
                    crypto::BlockHash::hash(t.data(), 16, ut.data());
                    crypto::BlockHash::hash(t.data()+16, sizeof(index_type), mask.data());
                    
                    
                    logger::log(logger::DBG) << "Derived token : " << hex_string(ut) << std::endl;
                    logger::log(logger::DBG) << "Mask : " << hex_string(mask) << std::endl;
                    
                    bool found = edb_.get(ut,r);
                    
                    if (found) {
                        logger::log(logger::DBG) << "Found: " << std::hex << r << std::endl;
                        
                        r = xor_mask(r, mask);
                        
                        post_callback(r);
                    }else{
                        logger::log(logger::ERROR) << "We were supposed to find something!" << std::endl;
                    }
                    
                }
            }
        }

        std::list<index_type> DianeServer::search_parallel(const SearchRequest& req, uint8_t derivation_threads_count,uint8_t access_threads_count)
        {
            std::list<index_type> results;
            
            auto callback = [&results](index_type i)
            {
                results.push_back(i);
            };
            
            search_parallel(req, callback, derivation_threads_count, access_threads_count);
            
            return results;
        }
        

        void DianeServer::search_parallel(const SearchRequest& req, std::function<void(index_type)> post_callback, uint8_t derivation_threads_count,uint8_t access_threads_count)
        {
            logger::log(logger::DBG) << "Expected matches: " << req.add_count << std::endl;
            logger::log(logger::DBG) << "Number of search nodes: " << req.token_list.size() << std::endl;
            
            auto derivation_prf = crypto::Prf<kUpdateTokenSize>(&req.kw_token);
            
            ThreadPool access_pool(access_threads_count);
            ThreadPool derive_pool(derivation_threads_count);


            auto lookup_job = [this, &post_callback](const update_token_type &ut, const std::array<uint8_t, sizeof(index_type)> &mask)
            {
                index_type r;
                
                
                bool found = edb_.get(ut,r);
                
                if (found) {
                    logger::log(logger::DBG) << "Found: " << std::hex << r << std::endl;
                    
                    post_callback(xor_mask(r, mask));
                }else{
                    logger::log(logger::ERROR) << "We were supposed to find something!" << std::endl;
                }
            };
            
            std::function<void(TokenTree::token_type, uint8_t)> derive_job = [&derive_job, &access_pool, &lookup_job, &derive_pool](TokenTree::token_type t, uint8_t d)
            {
                TokenTree::token_type st = TokenTree::derive_leftmost_node(t, d, derive_job);
                
                logger::log(logger::DBG) << "Derived leaf token: " << hex_string(st) << std::endl;

                // apply the hash function
                
                // derive the two parts of the leaf search token
                // it avoids having to use some different IVs to have two different hash functions.
                // it might decrease the security bounds by a few bits, but, meh ...
                update_token_type ut;
                std::array<uint8_t, sizeof(index_type)> mask;

                crypto::BlockHash::hash(st.data(), 16, ut.data());
                crypto::BlockHash::hash(st.data()+16, sizeof(index_type), mask.data());
                
                
                logger::log(logger::DBG) << "Derived token : " << hex_string(ut) << std::endl;
                logger::log(logger::DBG) << "Mask : " << hex_string(mask) << std::endl;
                
                access_pool.enqueue(lookup_job, ut, mask);

            };
            
            
            
            
            for (auto it_token = req.token_list.begin(); it_token != req.token_list.end(); ++it_token) {
                
                logger::log(logger::DBG) << "Search token key: " << hex_string(it_token->first) << std::endl;
                logger::log(logger::DBG) << "Search token depth: " << std::dec << (uint32_t)(it_token->second) << std::endl;

                
                // post the derivation job
                
                derive_pool.enqueue(derive_job, it_token->first, it_token->second);
            }
            
            // wait for the pools to finish
            derive_pool.join();
            access_pool.join();
        }

        void DianeServer::update(const UpdateRequest& req)
        {
            logger::log(logger::DBG) << "Update: (" << hex_string(req.token) << ", " << std::hex << req.index << ")" << std::endl;
            
            edb_.put(req.token, req.index);
        }
        
        std::ostream& DianeServer::print_stats(std::ostream& out) const
        {
            return out;
        }

    }
}