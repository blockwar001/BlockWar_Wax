#include <RollGame.hpp>

RollGame::RollGame(const name &receiver, const name &code, const datastream<const char *> &ds)
   : contract(receiver, code, ds)
   , __sendids(receiver, receiver.value)
   , __tasks(receiver, receiver.value)
{
}

ACTION RollGame::randtest(const name &actor, const checksum256 &rand_value)
{
   require_auth(actor);

   __send_transaction<randtestlog_action>(actor, actor, rand_value, __rand_res(rand_value));
}

ACTION RollGame::randtestlog(const name &actor, const checksum256 &rand_value, uint8_t res)
{
   require_auth(get_self());
   require_recipient(actor);
}

ACTION RollGame::requesttask(uint64_t id, const checksum256 &hash_seed)
{
   require_auth(SERVER_ACCOUNT);

   auto itr = __tasks.find(id);
   check(itr == __tasks.end(), "id already exists");

   __tasks.emplace(get_self(), [&](task &it)
   {
      it.id = id;
      it.ctime = current_time_point();
      it.hash_seed = hash_seed;
      it.res = 0;
   });

   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "request"_n,
      tuple{get_self(), hash_seed, id}
   ).send();

   //异步删除过期数据
   __send_transaction<clearress_action>(get_self(), string(""));
}

ACTION RollGame::dotask(uint64_t id, const string &seed)
{
   require_auth(SERVER_ACCOUNT);

   auto itr = __tasks.find(id);
   check(itr != __tasks.end(), "task dont exist");
   check(itr->rnum == checksum256(), "task has been done");

   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "create"_n,
      tuple{seed, itr->hash_seed}
   ).send();
}

ACTION RollGame::receiverand(uint64_t assoc_id, const checksum256 &random_value)
{
   require_auth(CREATE_RANDOM_ACCOUNT);

   auto itr = __tasks.find(assoc_id);
   check(itr != __tasks.end(), "task dont exist");

   uint8_t res = __rand_res(random_value);
   __tasks.modify(itr, get_self(), [&](task &it)
   {
      it.rnum = random_value;
      it.res = res; 
   });
   
   //日志
   action(
      {get_self(), "active"_n},
      get_self(),
      "dotasklog"_n,
      tuple{assoc_id, itr->rnum, res}
   ).send();
}

ACTION RollGame::dotasklog(uint64_t id, const checksum256 &rand_value, uint8_t res)
{
   require_auth(get_self());
}

ACTION RollGame::betlog(uint64_t gid, uint64_t uid, vector<bet_item> &bets)
{
   require_auth(BET_LOG_ACCOUNT);
}

ACTION RollGame::clearress(const string &memo)
{
   require_auth(get_self());

   auto index = __tasks.get_index<"bytime"_n>();
   auto itr = index.begin();

   uint16_t count = 0;
   while (itr != index.end())
   {
      if (current_time_point() > itr->ctime + hours(1))
      {
         count++;
         itr = index.erase(itr);
         if (count > 100)
         {
            //一次不能删除太多，否则会执行失败
            break;
         }
      }
      else
      {
         break;
      }
   }
}

uint32_t RollGame::__create_sender_id()
{
   auto info = __sendids.get_or_default({1234});
   info.id++;
   __sendids.set(info, get_self());

   return info.id;
}

template <typename Action, typename... Args>
void RollGame::__send_transaction(const name &payer, Args &&...args)
{
   Action action(get_self(), {get_self(), "active"_n});
   transaction txn{};
   txn.delay_sec = 0;
   txn.actions.emplace_back(action.to_action(forward<Args>(args)...));
   txn.send(__create_sender_id(), payer, true);
}

uint8_t RollGame::__rand_res(const checksum256 &rand_value)
{
   auto rbytes = rand_value.extract_as_byte_array();

   uint32_t rand_num = rbytes[6];
   rand_num <<= 8;
   rand_num |= rbytes[11];
   rand_num <<= 8;
   rand_num |= rbytes[17];
   rand_num <<= 8;
   rand_num |= rbytes[29];

   return rand_num % 6 + 1;
}
