#include <eosio/eosio.hpp>
#include <string>

using namespace eosio;
using namespace std;

CONTRACT RandCreator : public contract {
   public:
      using contract::contract;

      RandCreator(const name &receiver, const name &code, const datastream<const char *> &ds);

      ACTION request(const name &caller, const checksum256 &hash_seed, uint64_t assoc_id);
      ACTION create(const string &seed, const checksum256 &hash_seed);

   private:
      TABLE job
      {  
         uint64_t    id;
         name        caller;
         checksum256 hash_seed;
         uint32_t    current_time;
         int         tapos_block_prefix;
         int         tapos_block_num;
         uint64_t    assoc_id;
         
         uint64_t primary_key() const { return id; }
         checksum256 by_seed() const { return hash_seed; }
         
         EOSLIB_SERIALIZE(job, (id)(caller)(hash_seed)(current_time)(tapos_block_prefix)(tapos_block_num)(assoc_id))
      };
      using jobs_table = multi_index<"rajobs"_n, job, indexed_by<"byseed"_n, const_mem_fun<job, checksum256, &job::by_seed>>>;
      jobs_table __jobs;

      bool check_seed(const eosio::checksum256 &hash_seed, const string &seed);
};