#include <RaceGame.hpp>

RaceGame::RaceGame(const name &receiver, const name &code, const datastream<const char *> &ds)
    : contract(receiver, code, ds), __sendids(receiver, receiver.value), __tasks(receiver, receiver.value)
{
}

ACTION RaceGame::randtest(const name &actor, const vector<item_info> &runners, const checksum256 &rand_value)
{
   require_auth(actor);

   result_info res;
   __rand_res(runners, rand_value, res);

   //日志
   __send_transaction<randtestlog_action>(actor, actor, runners, rand_value, res);
}

ACTION RaceGame::randtestlog(const name &actor, const vector<item_info> &runners, const checksum256 &rand_value, const result_info &res)
{
   require_auth(get_self());
   require_recipient(actor);
}

ACTION RaceGame::requesttask(uint32_t match_id, uint32_t round_id, const vector<item_info> &runners, const checksum256 &hash_seed)
{
   require_auth(SERVER_ACCOUNT);

   uint64_t game_id = __get_game_id(match_id, round_id);
   auto itr = __tasks.find(game_id);
   check(itr == __tasks.end(), "id already exists");

   __tasks.emplace(get_self(), [&](task &it)
   {
      it.match_id = match_id;
      it.round_id = round_id;
      it.runners = runners;
      it.ctime = current_time_point();
      it.hash_seed = hash_seed; 
   });

   //请求生产随机数
   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "request"_n,
      tuple{get_self(), hash_seed, game_id}
   ).send();

   //异步删除过期数据
   __send_transaction<clearress_action>(get_self(), string(""));
}

ACTION RaceGame::dotask(uint32_t match_id, uint32_t round_id, const string &seed)
{
   require_auth(SERVER_ACCOUNT);

   uint64_t game_id = __get_game_id(match_id, round_id);
   auto itr = __tasks.find(game_id);
   check(itr != __tasks.end(), "task dont exist");

   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "create"_n,
      tuple{seed, itr->hash_seed}
   ).send();
}

ACTION RaceGame::receiverand(uint64_t assoc_id, const checksum256 &random_value)
{
   require_auth(CREATE_RANDOM_ACCOUNT);
   auto itr = __tasks.find(assoc_id);
   check(itr != __tasks.end(), "task dont exist");
   check(itr->rnum == checksum256(), "task has been done");

   result_info res;
   __rand_res(itr->runners, random_value, res);

   __tasks.modify(itr, get_self(), [&](task &it)
   {
      it.rnum = random_value;
      it.result = res; 
   });

   //日志
   action(
      {get_self(), "active"_n},
      get_self(),
      "dotasklog"_n,
      tuple{itr->match_id, itr->round_id, itr->runners, random_value, res}
   ).send();
}

ACTION RaceGame::dotasklog(uint32_t match_id, uint32_t round_id, const vector<item_info> &runners, const checksum256 &rand_value, const result_info &res)
{
   require_auth(get_self());
}

ACTION RaceGame::clearress(const string &memo)
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

ACTION RaceGame::betlog(uint32_t match_id, uint32_t round_id, vector<user_bet_items> &bets)
{
   require_auth(BET_LOG_ACCOUNT);
}

uint32_t RaceGame::__create_sender_id()
{
   auto info = __sendids.get_or_default({1234});
   info.id++;
   __sendids.set(info, get_self());

   return info.id;
}

template <typename Action, typename... Args>
void RaceGame::__send_transaction(const name &payer, Args &&...args)
{
   Action action(get_self(), {get_self(), "active"_n});
   transaction txn{};
   txn.delay_sec = 0;
   txn.actions.emplace_back(action.to_action(forward<Args>(args)...));
   txn.send(__create_sender_id(), payer, true);
}

uint64_t RaceGame::__get_game_id(uint32_t match_id, uint32_t round_id)
{
   uint64_t game_id = match_id;
   game_id <<= 32;
   game_id |= round_id;
   return game_id;
}

uint32_t RaceGame::__create_uint32(uint8_t num1, uint8_t num2, uint8_t num3, uint8_t num4)
{
   uint32_t res = num1;
   res <<= 8;
   res |= num2;
   res <<= 8;
   res |= num3;
   res <<= 8;
   res |= num4;
   return res;
}

void RaceGame::__checksum256_to_uint32s(const checksum256 &rand_value, uint32_t *nums)
{
   auto rbytes = rand_value.extract_as_byte_array();
   for (int i = 0; i < 8; i++)
   {
      nums[i] = __create_uint32(rbytes[i * 4], rbytes[i * 4 + 1], rbytes[i * 4 + 2], rbytes[i * 4 + 3]);
   }
}

uint8_t RaceGame::__rand_2_3_4(uint32_t rnum)
{
   uint32_t hit_num = rnum % 100;
   if (hit_num < 70)
   {
      return 2;
   }
   else if (hit_num < 95)
   {
      return 3;
   }
   else
   {
      return 4;
   }
}

void RaceGame::__rand_res(const vector<item_info> &runners, const checksum256 &rand_value, result_info &res)
{
   uint8_t phoenix_count;         // runner中是否包含凤凰
   item_info phoenix_item;        // contain_phoenix==true时，凤凰的信息
   uint32_t total_wei;            // runners的总权重，不包括凤凰
   vector<item_weight> item_weis; // runners权重表，不包括凤凰

   __parse_runner(runners, phoenix_count, phoenix_item, total_wei, item_weis);
   check(phoenix_count <= 1, "only contain one phoenix runner");

   uint32_t nums[8];
   __checksum256_to_uint32s(rand_value, nums);
   __rand_res_type(phoenix_count == 1, res.type, nums[0]);

   res.multiple = 0;
   switch (res.type.value)
   {
   //普通结果
   case result_info::RESULT_TYPE_ID_NORMAL.value:
      __rand_items(item_weis, 1, total_wei, res.items, nums + 1);
      break;

   //凤凰特效
   case result_info::RESULT_TYPE_ID_PHOENIX.value:
      check(phoenix_count == 1, "dont contain phoenix runner");
      res.items.push_back(phoenix_item);
      break;

   //多个动物并列第一
   case result_info::RESULT_TYPE_ID_SEVERAL.value:
      __rand_items(item_weis, __rand_2_3_4(nums[1]), total_wei, res.items, nums + 2);
      break;

   //多倍特效
   case result_info::RESULT_TYPE_ID_MULTIPLE.value:
      __rand_items(item_weis, 1, total_wei, res.items, nums + 2);
      res.multiple = __rand_2_3_4(nums[1]);
      res.items[0].multiple *= res.multiple;
      break;

   //团灭特效
   case result_info::RESULT_TYPE_ID_DEATH.value:
      break;

   //全赢特效
   case result_info::RESULT_TYPE_ID_ALLWIN.value:
   {
      vector<item_weight>::iterator itr = item_weis.begin();
      while (itr != item_weis.end())
      {
         res.items.push_back(itr->runner);
         itr++;
      }

      //包括凤凰
      if (phoenix_count == 1)
      {
         res.items.push_back(phoenix_item);
      }
      break;
   }

   default:
      check(false, "unknown type");
      break;
   }
}

void RaceGame::__parse_runner(const vector<item_info> &runners, uint8_t &phoenix_count, item_info &phoenix_item, uint32_t &total_wei, vector<item_weight> &item_weis)
{
   phoenix_count = 0;
   total_wei = 0;

   uint32_t total_multiple = 1;
   vector<item_info>::const_iterator itr = runners.begin();
   while (itr != runners.end())
   {
      check(itr->multiple > 0, "multiple must > 0");

      if (itr->id.value == item_info::ITEM_ID_PHOENIX.value)
      {
         phoenix_count++;
         phoenix_item.id = itr->id;
         phoenix_item.multiple = itr->multiple;
         phoenix_item.level = itr->level;
         phoenix_item.item_id = itr->item_id;
      }
      else
      {
         total_multiple *= itr->multiple;
         item_weis.push_back({{
            itr->id,
            itr->item_id,
            itr->multiple,
            itr->level
         }, 0});
      }

      itr++;
   }

   vector<item_weight>::iterator item_itr = item_weis.begin();
   while (item_itr != item_weis.end())
   {
      item_itr->weight = total_multiple / item_itr->runner.multiple;
      total_wei += item_itr->weight;

      item_itr++;
   }
}

void RaceGame::__rand_res_type(bool contain_phoenix, name &type, uint32_t rnum)
{
   uint32_t total_weight = contain_phoenix ? result_info::WEIGHT_TOTAL : result_info::WEIGHT_TOTAL - result_info::WEIGHT_TYPE_PHOENIX;
   uint32_t hit_weight = rnum % total_weight;

   //普通结果
   uint32_t current_weight = result_info::WEIGHT_TYPE_NORMAL;
   if (hit_weight < current_weight)
   {
      type = result_info::RESULT_TYPE_ID_NORMAL;
      return;
   }

   //凤凰特效
   if (contain_phoenix)
   {
      current_weight += result_info::WEIGHT_TYPE_PHOENIX;
      if (hit_weight < current_weight)
      {
         type = result_info::RESULT_TYPE_ID_PHOENIX;
         return;
      }
   }

   //多个动物并列第一
   current_weight += result_info::WEIGHT_TYPE_SEVERAL;
   if (hit_weight < current_weight)
   {
      type = result_info::RESULT_TYPE_ID_SEVERAL;
      return;
   }

   //多倍特效
   current_weight += result_info::WEIGHT_TYPE_MULTIPLE;
   if (hit_weight < current_weight)
   {
      type = result_info::RESULT_TYPE_ID_MULTIPLE;
      return;
   }

   //团灭特效
   current_weight += result_info::WEIGHT_TYPE_DEATH;
   if (hit_weight < current_weight)
   {
      type = result_info::RESULT_TYPE_ID_DEATH;
      return;
   }

   //全赢特效
   type = result_info::RESULT_TYPE_ID_ALLWIN;
}

void RaceGame::__rand_items(vector<item_weight> &item_weis, uint8_t count, uint32_t total_wei, vector<item_info> &items, uint32_t *nums)
{
   if (count == 0)
   {
      return;
   }

   uint32_t rnum = nums[0];
   uint32_t hit_weight = rnum % total_wei;
   uint32_t current_weight = 0;
   vector<item_weight>::iterator itr = item_weis.begin();
   while (itr != item_weis.end())
   {
      current_weight += itr->weight;
      if (hit_weight < current_weight)
      {
         items.push_back(itr->runner);
         total_wei -= itr->weight;
         break;
      }
      else
      {
         itr++;
      }
   }

   __rand_items(item_weis, count - 1, total_wei, items, nums + 1);
}