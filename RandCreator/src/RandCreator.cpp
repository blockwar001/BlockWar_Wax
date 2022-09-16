#include <RandCreator.hpp>
#include <eosio/crypto.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>

RandCreator::RandCreator(const name &receiver, const name &code, const datastream<const char *> &ds)
   : contract(receiver, code, ds)
   , __jobs(receiver, receiver.value)
{
}

ACTION RandCreator::request(const name &caller, const checksum256 &hash_seed, uint64_t assoc_id)
{
   require_auth(caller);

   auto index = __jobs.get_index<"byseed"_n>();
   auto it = index.find(hash_seed);
   check(it == index.end(), "seed already exists");

   __jobs.emplace(caller, [&](job &it){
      it.id = __jobs.available_primary_key();
      it.caller = caller;
      it.current_time = current_time_point().sec_since_epoch();
      it.hash_seed = hash_seed;
      it.tapos_block_num = tapos_block_num();
      it.tapos_block_prefix = tapos_block_prefix();
      it.assoc_id = assoc_id;
   });
}

ACTION RandCreator::create(const string &seed, const checksum256 &hash_seed)
{
   auto index = __jobs.get_index<"byseed"_n>();
   auto it = index.find(hash_seed);
   check(it != index.end(), "seed dont exist");

   require_auth(it->caller);
   check(check_seed(hash_seed, seed), "seed and hash_seed mismatch");

   string mixd = "";
   mixd.append(seed);
   mixd.append(to_string(it->tapos_block_num * it->tapos_block_prefix - it->current_time + it->id));
   mixd.append(it->caller.to_string());
   
   checksum256 result = sha256(mixd.data(), mixd.size());

   action(
      {get_self(), "active"_n}, 
      it->caller, 
      "receiverand"_n,
      tuple(it->assoc_id, result)
   ).send();

   index.erase(it);
}

bool RandCreator::check_seed(const eosio::checksum256 &hash_seed, const string &seed)
{
   return hash_seed == sha256(seed.data(), seed.size());
}