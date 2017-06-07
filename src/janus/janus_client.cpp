//
//  janus_client.cpp
//  sophos
//
//  Created by Raphael Bost on 14/05/2017.
//  Copyright © 2017 Raphael Bost. All rights reserved.
//

#include "janus_client.hpp"

namespace sse {
    namespace janus {
        
        inline std::string keyword_doc_string(const std::string &kw, index_type ind)
        {
            return hex_string(ind) + "||" + kw;
        }
        
        std::string JanusClient::tag_derivation_key() const
        {
            return master_prf_.prf_string("tag_derivation");
        }
        
        std::string JanusClient::punct_enc_key() const
        {
            return master_prf_.prf_string("punct_enc");
        }
        
        std::string JanusClient::insertion_derivation_master_key() const
        {
            return master_prf_.prf_string("add_derivation_master_key");
        }
        
        std::string JanusClient::insertion_kw_token_master_key() const
        {
            return master_prf_.prf_string("add_kw_token_master_key");
        }
        
        std::string JanusClient::deletion_derivation_master_key() const
        {
            return master_prf_.prf_string("del_derivation_master_key");
        }
        
        std::string JanusClient::delertion_kw_token_master_key() const
        {
            return master_prf_.prf_string("del_kw_token_master_key");
        }
        
        JanusClient::JanusClient(const std::string& add_map_path, const std::string& del_map_path) :
        master_prf_(),
        tag_prf_(tag_derivation_key()),
        punct_enc_master_prf_(punct_enc_key()),
        insertion_client_(add_map_path, insertion_derivation_master_key(), insertion_kw_token_master_key()),
        deletion_client_(del_map_path, deletion_derivation_master_key(), delertion_kw_token_master_key())
        {
        }

        JanusClient::JanusClient(const std::string& add_map_path, const std::string& del_map_path, const std::string& master_key) :
            master_prf_(master_key),
            tag_prf_(tag_derivation_key()),
            punct_enc_master_prf_(punct_enc_key()),
            insertion_client_(add_map_path, insertion_derivation_master_key(), insertion_kw_token_master_key()),
            deletion_client_(del_map_path, deletion_derivation_master_key(), delertion_kw_token_master_key())
        {
            std::cout << "MASTER KEY: " << hex_string(master_key) << "\n";
        }

//        JanusClient::~JanusClient();
        
        
        
        SearchRequest JanusClient::search_request(const std::string &keyword) const
        {
            SearchRequest req;
            
            req.insertion_search_request = insertion_client_.search_request(keyword);
            req.deletion_search_request = deletion_client_.search_request(keyword, false); // do not log if there is no deletion

            
            // the key derivation will to be modified for the real implementation
            auto punct_encryption = crypto::PuncturableEncryption(punct_enc_master_prf_.prf(keyword));
            req.first_key_share = punct_encryption.initial_keyshare(req.deletion_search_request.add_count); // the add_count for the deletion scheme is actually the number of deleted entries
            
            if (req.insertion_search_request.add_count < req.deletion_search_request.add_count) {
                logger::log(logger::ERROR) << "Keyword " << keyword << " was inserted " << req.insertion_search_request.add_count << " times and deleted " << req.deletion_search_request.add_count << " times" << std::endl;
            }
            
            return req;
        }
        
        InsertionRequest    JanusClient::insertion_request(const std::string &keyword, const index_type index)
        {
            auto punct_encryption = crypto::PuncturableEncryption(punct_enc_master_prf_.prf(keyword));
            
            crypto::punct::tag_type tag = tag_prf_.prf(keyword_doc_string(keyword, index));
            crypto::punct::ciphertext_type ct = punct_encryption.encrypt(index, tag);
            
            return insertion_client_.update_request(keyword, ct);
        }
        
        DeletionRequest    JanusClient::deletion_request(const std::string &keyword, const index_type index)
        {
            auto punct_encryption = crypto::PuncturableEncryption(punct_enc_master_prf_.prf(keyword));
            
            uint32_t n_del = deletion_client_.get_match_count(keyword);
            
            crypto::punct::tag_type tag = tag_prf_.prf(keyword_doc_string(keyword, index));
            crypto::punct::key_share_type ks = punct_encryption.inc_puncture(n_del+1, tag);
            
            return deletion_client_.update_request(keyword, ks);
        }
        

    }
}
