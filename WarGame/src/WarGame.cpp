#include <WarGame.hpp>

WarGame::WarGame(const name &receiver, const name &code, const datastream<const char *> &ds)
   : contract(receiver, code, ds)
   , __sendids(receiver, receiver.value)
   , __tasks(receiver, receiver.value)
   , __taskids(receiver, receiver.value)
{
}

ACTION WarGame::initweights()
{
   require_auth(SERVER_ACCOUNT);

   __init_type_weights();
   __init_item_weights();
   __init_dragon_weights();
   __init_shoot_weights();
   __init_train_weights();
}

ACTION WarGame::clearress(const name &game)
{
   require_auth(get_self());

   resulttasks_table ress(get_self(), game.value);
   auto index = ress.get_index<"bytime"_n>();
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

ACTION WarGame::clearrnums(const string &memo)
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

ACTION WarGame::randtest(const name &actor, const name &game, const checksum256 &rand_value)
{
   require_auth(actor);
   check(game.value == WAR_LOW_GAME_NAME.value || game.value == WAR_HIGH_GAME_NAME.value, "unknown game");

   result_info res;
   __rand_res(game, res, rand_value);

   //日志
   __send_transaction<randtestlog_action>(actor, actor, game, rand_value, res);
}

ACTION WarGame::randtestlog(const name &actor, const name &game, const checksum256 &rand_value, const result_info &res)
{
   require_auth(get_self());
   require_recipient(actor);
}

ACTION WarGame::requesttask(const game_info &game, const checksum256 &hash_seed)
{
   require_auth(SERVER_ACCOUNT);
   check(game.game.value == WAR_LOW_GAME_NAME.value || game.game.value == WAR_HIGH_GAME_NAME.value, "unknown game");

   uint64_t game_id = __get_game_id(game);

   //检查是否有重复的
   resulttasks_table resulttasks(get_self(), game.game.value);
   auto itr = resulttasks.find(game_id);
   check(itr == resulttasks.end(), "task already exists");

   //创建任务
   uint64_t task_id = __create_task_id(); //__tasks.available_primary_key();
   __tasks.emplace(get_self(), [&](task_request_rand &it)
   {
      it.id = task_id;
      it.game = game;
      it.hash_seed = hash_seed;
      it.ctime = current_time_point();
   });

   resulttasks.emplace(get_self(), [&](task_create_result &it)
   {
      it.task_id = task_id;
      it.game = game;
      //it.rnum = 0x00;
      it.ctime = current_time_point();
   });

   //请求生产随机数
   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "request"_n,
      tuple{get_self(), hash_seed, task_id}
   ).send();

   //异步删除过期数据
   __send_transaction<clearress_action>(get_self(), game.game);
   __send_transaction<clearrnums_action>(get_self(), string(""));
}

ACTION WarGame::dotask(const game_info &game, const string &seed)
{
   require_auth(SERVER_ACCOUNT);
   check(game.game.value == WAR_LOW_GAME_NAME.value || game.game.value == WAR_HIGH_GAME_NAME.value, "unknown game");

   uint64_t game_id = __get_game_id(game);

   resulttasks_table resulttasks(get_self(), game.game.value);
   auto itr = resulttasks.find(game_id);
   check(itr != resulttasks.end(), "task dont exist");
   check(itr->rnum == checksum256(), "task has been done");

   auto req_itr = __tasks.find(itr->task_id);
   check(req_itr != __tasks.end(), "task id dont exist");

   action(
      {get_self(), "active"_n},
      CREATE_RANDOM_ACCOUNT,
      "create"_n,
      tuple{seed, req_itr->hash_seed}
   ).send();

}

ACTION WarGame::receiverand(uint64_t assoc_id, const checksum256 &random_value)
{
   require_auth(CREATE_RANDOM_ACCOUNT);

   auto req_itr = __tasks.find(assoc_id);
   check(req_itr != __tasks.end(), "task dont exist");

   uint64_t game_id = __get_game_id(req_itr->game);

   resulttasks_table resulttasks(get_self(), req_itr->game.game.value);
   auto res_itr = resulttasks.find(game_id);
   check(res_itr != resulttasks.end(), "task dont exist");

   result_info res;
   __rand_res(req_itr->game.game, res, random_value);

   resulttasks.modify(res_itr, get_self(), [&](task_create_result &it)
   {
      it.rnum = random_value;
      it.result = res;
   });

   //日志
   action(
      {get_self(), "active"_n},
      get_self(),
      "dotasklog"_n,
      tuple{req_itr->game, random_value, res}
   ).send();
   
}

ACTION WarGame::dotasklog(const game_info &game, const checksum256 &rand_value, const result_info &res)
{
   require_auth(get_self());
}

ACTION WarGame::betlog(const game_info &game, vector<user_bet_items> &bets)
{
   require_auth(BET_LOG_ACCOUNT);
}

uint32_t WarGame::__create_sender_id()
{
   auto info = __sendids.get_or_default({1234});
   info.id++;
   __sendids.set(info, get_self());

   return info.id;
}

uint32_t WarGame::__create_task_id()
{
   auto info = __taskids.get_or_default({1});
   info.id++;
   __taskids.set(info, get_self());

   return info.id;
}

template <typename Action, typename... Args>
void WarGame::__send_transaction(const name &payer, Args &&...args)
{
   Action action(get_self(), {get_self(), "active"_n});
   transaction txn{};
   txn.delay_sec = 0;
   txn.actions.emplace_back(action.to_action(forward<Args>(args)...));
   txn.send(__create_sender_id(), payer, true);
}

uint64_t WarGame::__get_game_id(const game_info &game)
{
   uint64_t game_id = game.match_id;
   game_id <<= 32;
   game_id |= game.round_id;
   return game_id;
}

void WarGame::__add_to_type_weights(restypeweis_table &table, const name &type, uint32_t weight)
{
   table.emplace(get_self(), [&](result_type_weight &it)
                 {
      it.type = type;
      it.weight = weight; });
}

void WarGame::__init_type_weights()
{
   //低倍
   restypeweis_table low_table(get_self(), WAR_LOW_GAME_NAME.value);
   check(low_table.begin() == low_table.end(), "already initialized");

   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_NORMAL, 1129037); //普通结果
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_DRAGONL, 39520);  //龙
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_SKELETON, 21165); //骷髅
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_SHOOT, 12450);    //打枪
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_TRAIN, 12450);    //火车
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_MULTIPLE, 12450); //多倍
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_ORC, 1245);       //小四喜-兽族
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_TERRAN, 1245);    //小四喜-人族
   __add_to_type_weights(low_table, result_info::RESULT_TYPE_ID_ANGEL, 498);      //大三元

   //高倍
   restypeweis_table high_table(get_self(), WAR_HIGH_GAME_NAME.value);
   check(high_table.begin() == high_table.end(), "already initialized");

   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_NORMAL, 3349116);  //普通结果
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_DRAGONL, 93000);   //龙
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_SKELETON, 102270); //骷髅
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_SHOOT, 24350);     //打枪
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_TRAIN, 24350);     //火车
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_MULTIPLE, 24350);  //多倍
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_ORC, 2435);        //小四喜-兽族
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_TERRAN, 2435);     //小四喜-人族
   __add_to_type_weights(high_table, result_info::RESULT_TYPE_ID_ANGEL, 974);       //大三元
}

void WarGame::__add_to_item_weights(itemweis_table &table, const name &id, uint32_t weight, uint8_t multiple, uint8_t index)
{
   table.emplace(get_self(), [&](item_weight &it)
   {
      it.item.id = id;
      it.item.index = index;
      it.item.multiple = multiple;
      it.weight = weight; 
   });
}

void WarGame::__init_item_weights()
{
   //低倍
   itemweis_table low_table(get_self(), WAR_LOW_GAME_NAME.value);
   check(low_table.begin() == low_table.end(), "already initialized");

   __add_to_item_weights(low_table, item_info::ITEM_ID_ORC_ORCS, 4, 6, 1);   //兽族-兽人
   __add_to_item_weights(low_table, item_info::ITEM_ID_ORC_SHAMAN, 3, 8, 2); //兽族-萨满
   __add_to_item_weights(low_table, item_info::ITEM_ID_ORC_TAUREN, 3, 8, 3); //兽族-牛头人
   __add_to_item_weights(low_table, item_info::ITEM_ID_ORC_CHIEF, 2, 12, 4); //兽族-酋长

   __add_to_item_weights(low_table, item_info::ITEM_ID_TERRAN_KING, 2, 12, 14);    //人族-国王
   __add_to_item_weights(low_table, item_info::ITEM_ID_TERRAN_KNIGHT, 3, 8, 15);   //人族-骑士
   __add_to_item_weights(low_table, item_info::ITEM_ID_TERRAN_MINISTER, 3, 8, 16); //人族-牧师
   __add_to_item_weights(low_table, item_info::ITEM_ID_TERRAN_WARRIOR, 4, 6, 17);  //人族-战士

   //高倍
   itemweis_table high_table(get_self(), WAR_HIGH_GAME_NAME.value);
   check(high_table.begin() == high_table.end(), "already initialized");

   __add_to_item_weights(high_table, item_info::ITEM_ID_ORC_ORCS, 6, 4, 1);    //兽族-兽人
   __add_to_item_weights(high_table, item_info::ITEM_ID_ORC_SHAMAN, 3, 8, 2);  //兽族-萨满
   __add_to_item_weights(high_table, item_info::ITEM_ID_ORC_TAUREN, 2, 12, 3); //兽族-牛头人
   __add_to_item_weights(high_table, item_info::ITEM_ID_ORC_CHIEF, 1, 24, 4);  //兽族-酋长

   __add_to_item_weights(high_table, item_info::ITEM_ID_TERRAN_KING, 1, 24, 14);    //人族-国王
   __add_to_item_weights(high_table, item_info::ITEM_ID_TERRAN_KNIGHT, 2, 12, 15);  //人族-骑士
   __add_to_item_weights(high_table, item_info::ITEM_ID_TERRAN_MINISTER, 3, 8, 16); //人族-牧师
   __add_to_item_weights(high_table, item_info::ITEM_ID_TERRAN_WARRIOR, 6, 4, 17);  //人族-战士
}

void WarGame::__add_to_dragon_weights(dragonweis_table &table, uint8_t min_multiple, uint8_t max_multiple, uint32_t weight)
{
   table.emplace(get_self(), [&](dragon_weights &it)
   {
      it.min_multiple = min_multiple;
      it.max_multiple = max_multiple;
      it.weight = weight; 
   });
}

void WarGame::__init_dragon_weights()
{
   //低倍
   dragonweis_table low_table(get_self(), WAR_LOW_GAME_NAME.value);
   check(low_table.begin() == low_table.end(), "already initialized");

   __add_to_dragon_weights(low_table, 24, 24, 48);
   __add_to_dragon_weights(low_table, 30, 60, 12);
   __add_to_dragon_weights(low_table, 61, 79, 3);
   __add_to_dragon_weights(low_table, 80, 100, 1);

   //高倍
   dragonweis_table high_table(get_self(), WAR_HIGH_GAME_NAME.value);
   check(high_table.begin() == high_table.end(), "already initialized");

   __add_to_dragon_weights(high_table, 30, 40, 100);
   __add_to_dragon_weights(high_table, 41, 59, 20);
   __add_to_dragon_weights(high_table, 60, 80, 4);
   __add_to_dragon_weights(high_table, 81, 99, 1);
}

void WarGame::__add_to_shoot_weights_2(shootweis_table &table, item_weight_info &item1, item_weight_info &item2)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
      };
      it.weight = item1.weight + item2.weight; 
   });
}

void WarGame::__add_to_shoot_weights_3(shootweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
         item3.item,
      };
      it.weight = item1.weight + item2.weight + item3.weight; 
   });
}

void WarGame::__add_to_shoot_weights_4(shootweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3, item_weight_info &item4)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
         item3.item,
         item4.item,
      };
      it.weight = item1.weight + item2.weight + item3.weight + item4.weight; 
   });
}

void WarGame::__init_shoot_weights()
{
   //高倍
   item_weight_info low_items[] =
   {
      {{item_info::ITEM_ID_ORC_ORCS, 6, 1}, 2},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 2}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 8, 3}, 3},
      {{item_info::ITEM_ID_ORC_CHIEF, 12, 4}, 4},

      {{item_info::ITEM_ID_TERRAN_KING, 12, 14}, 4},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 8, 15}, 3},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 16}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 6, 17}, 2},
   };

   shootweis_table low_table_2(get_self(), items_weights::SHOOT_LOW_2.value);
   check(low_table_2.begin() == low_table_2.end(), "already initialized");

   shootweis_table low_table_3(get_self(), items_weights::SHOOT_LOW_3.value);
   check(low_table_3.begin() == low_table_3.end(), "already initialized");

   shootweis_table low_table_4(get_self(), items_weights::SHOOT_LOW_4.value);
   check(low_table_4.begin() == low_table_4.end(), "already initialized");

   //低倍
   item_weight_info high_items[] =
   {
      {{item_info::ITEM_ID_ORC_ORCS, 4, 1}, 6},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 2}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 12, 3}, 2},
      {{item_info::ITEM_ID_ORC_CHIEF, 24, 4}, 1},

      {{item_info::ITEM_ID_TERRAN_KING, 24, 14}, 1},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 12, 15}, 2},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 16}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 4, 17}, 6},
   };

   shootweis_table high_table_2(get_self(), items_weights::SHOOT_HIGH_2.value);
   check(high_table_2.begin() == high_table_2.end(), "already initialized");

   shootweis_table high_table_3(get_self(), items_weights::SHOOT_HIGH_3.value);
   check(high_table_3.begin() == high_table_3.end(), "already initialized");

   shootweis_table high_table_4(get_self(), items_weights::SHOOT_HIGH_4.value);
   check(high_table_4.begin() == high_table_4.end(), "already initialized");

   for (int8_t a = 0; a < 8; a++)
   {
      for (int8_t b = a + 1; b < 8; b++)
      {
         // 2枪
         __add_to_shoot_weights_2(low_table_2, low_items[a], low_items[b]);
         __add_to_shoot_weights_2(high_table_2, high_items[a], high_items[b]);

         for (int8_t c = b + 1; c < 8; c++)
         {
            // 3枪
            __add_to_shoot_weights_3(low_table_3, low_items[a], low_items[b], low_items[c]);
            __add_to_shoot_weights_3(high_table_3, high_items[a], high_items[b], high_items[c]);

            for (int8_t d = c + 1; d < 8; d++)
            {
               // 4枪
               __add_to_shoot_weights_4(low_table_4, low_items[a], low_items[b], low_items[c], low_items[d]);
               __add_to_shoot_weights_4(high_table_4, high_items[a], high_items[b], high_items[c], high_items[d]);
            }
         }
      }
   }
}

void WarGame::__add_to_train_weights_2(trainweis_table &table, item_weight_info &item1, item_weight_info &item2)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
      };
      it.weight = item1.weight + item2.weight; 
   });
}

void WarGame::__add_to_train_weights_3(trainweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
         item3.item,
      };
      it.weight = item1.weight + item2.weight + item3.weight; 
   });
}

void WarGame::__add_to_train_weights_4(trainweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3, item_weight_info &item4)
{
   table.emplace(get_self(), [&](items_weights &it)
   {
      it.id = table.available_primary_key();
      it.items = 
      {
         item1.item,
         item2.item,
         item3.item,
         item4.item,
      };
      it.weight = item1.weight + item2.weight + item3.weight + item4.weight; 
   });
}

void WarGame::__init_train_weights()
{
   //高倍
   item_weight_info low_orc_items[] =
   {
      {{item_info::ITEM_ID_ORC_ORCS, 6, 1}, 2},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 2}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 8, 3}, 3},
      {{item_info::ITEM_ID_ORC_CHIEF, 12, 4}, 4},

      {{item_info::ITEM_ID_ORC_ORCS, 6, 5}, 2},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 6}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 8, 7}, 3},
      {{item_info::ITEM_ID_ORC_CHIEF, 12, 8}, 4},

      {{item_info::ITEM_ID_ORC_ORCS, 6, 9}, 2},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 10}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 8, 11}, 3},
      {{item_info::ITEM_ID_ORC_CHIEF, 12, 12}, 4},
   };

   item_weight_info low_terran_items[] =
   {
      {{item_info::ITEM_ID_TERRAN_KING, 12, 14}, 4},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 8, 15}, 3},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 16}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 6, 17}, 2},

      {{item_info::ITEM_ID_TERRAN_KING, 12, 18}, 4},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 8, 19}, 3},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 20}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 6, 21}, 2},

      {{item_info::ITEM_ID_TERRAN_KING, 12, 22}, 4},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 8, 23}, 3},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 24}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 6, 25}, 2},
   };

   trainweis_table low_table_2(get_self(), items_weights::TRAIN_LOW_2.value);
   check(low_table_2.begin() == low_table_2.end(), "already initialized");

   trainweis_table low_table_3(get_self(), items_weights::TRAIN_LOW_3.value);
   check(low_table_3.begin() == low_table_3.end(), "already initialized");

   trainweis_table low_table_4(get_self(), items_weights::TRAIN_LOW_4.value);
   check(low_table_4.begin() == low_table_4.end(), "already initialized");

   //低倍
   item_weight_info high_orc_items[] =
   {
      {{item_info::ITEM_ID_ORC_ORCS, 4, 1}, 6},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 2}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 12, 3}, 2},
      {{item_info::ITEM_ID_ORC_CHIEF, 24, 4}, 1},

      {{item_info::ITEM_ID_ORC_ORCS, 4, 5}, 6},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 6}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 12, 7}, 2},
      {{item_info::ITEM_ID_ORC_CHIEF, 24, 8}, 1},

      {{item_info::ITEM_ID_ORC_ORCS, 4, 9}, 6},
      {{item_info::ITEM_ID_ORC_SHAMAN, 8, 10}, 3},
      {{item_info::ITEM_ID_ORC_TAUREN, 12, 11}, 2},
      {{item_info::ITEM_ID_ORC_CHIEF, 24, 12}, 1},
   };

   item_weight_info high_terran_items[] =
   {
      {{item_info::ITEM_ID_TERRAN_KING, 24, 14}, 1},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 12, 15}, 2},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 16}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 4, 17}, 6},

      {{item_info::ITEM_ID_TERRAN_KING, 24, 18}, 1},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 12, 19}, 2},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 20}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 4, 21}, 6},

      {{item_info::ITEM_ID_TERRAN_KING, 24, 22}, 1},
      {{item_info::ITEM_ID_TERRAN_KNIGHT, 12, 23}, 2},
      {{item_info::ITEM_ID_TERRAN_MINISTER, 8, 24}, 3},
      {{item_info::ITEM_ID_TERRAN_WARRIOR, 4, 25}, 6},
   };

   trainweis_table high_table_2(get_self(), items_weights::TRAIN_HIGH_2.value);
   check(high_table_2.begin() == high_table_2.end(), "already initialized");

   trainweis_table high_table_3(get_self(), items_weights::TRAIN_HIGH_3.value);
   check(high_table_3.begin() == high_table_3.end(), "already initialized");

   trainweis_table high_table_4(get_self(), items_weights::TRAIN_HIGH_4.value);
   check(high_table_4.begin() == high_table_4.end(), "already initialized");

   for (int i = 0; i < 12; i++)
   {
      if (i + 1 < 12)
      {
         // 2节兽族车厢
         __add_to_train_weights_2(low_table_2, low_orc_items[i], low_orc_items[i + 1]);
         __add_to_train_weights_2(high_table_2, high_orc_items[i], high_orc_items[i + 1]);

         // 2节人族车厢
         __add_to_train_weights_2(low_table_2, low_terran_items[i], low_terran_items[i + 1]);
         __add_to_train_weights_2(high_table_2, high_terran_items[i], high_terran_items[i + 1]);

         if (i + 2 < 12)
         {
            // 3节兽族车厢
            __add_to_train_weights_3(low_table_3, low_orc_items[i], low_orc_items[i + 1], low_orc_items[i + 2]);
            __add_to_train_weights_3(high_table_3, high_orc_items[i], high_orc_items[i + 1], high_orc_items[i + 2]);

            // 3节人族车厢
            __add_to_train_weights_3(low_table_3, low_terran_items[i], low_terran_items[i + 1], low_terran_items[i + 2]);
            __add_to_train_weights_3(high_table_3, high_terran_items[i], high_terran_items[i + 1], high_terran_items[i + 2]);

            if (i + 3 < 12)
            {
               // 4节兽族车厢
               __add_to_train_weights_4(low_table_4, low_orc_items[i], low_orc_items[i + 1], low_orc_items[i + 2], low_orc_items[i + 3]);
               __add_to_train_weights_4(high_table_4, high_orc_items[i], high_orc_items[i + 1], high_orc_items[i + 2], high_orc_items[i + 3]);

               // 4节人族车厢
               __add_to_train_weights_4(low_table_4, low_terran_items[i], low_terran_items[i + 1], low_terran_items[i + 2], low_terran_items[i + 3]);
               __add_to_train_weights_4(high_table_4, high_terran_items[i], high_terran_items[i + 1], high_terran_items[i + 2], high_terran_items[i + 3]);
            }
         }
      }
   }
}

uint32_t WarGame::__create_uint32(uint8_t num1, uint8_t num2, uint8_t num3, uint8_t num4)
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

void WarGame::__checksum256_to_uint32s(const checksum256 &rand_value, uint32_t *nums)
{
   auto rbytes = rand_value.extract_as_byte_array();
   for (int i = 0; i < 8; i++)
   {
      nums[i] = __create_uint32(rbytes[i * 4], rbytes[i * 4 + 1], rbytes[i * 4 + 2], rbytes[i * 4 + 3]);
   }
}

uint8_t WarGame::__rand_2_3_4(uint32_t rnum)
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

uint8_t WarGame::__rand_item_index(uint8_t min_index, uint32_t rnum)
{
   return min_index + 4 * (rnum % 3);
}

void WarGame::__rand_res(const name &game, result_info &res, const checksum256 &rand_value)
{
   uint32_t nums[8];
   __checksum256_to_uint32s(rand_value, nums);

   __rand_res_type(game, res.type, nums[0]);
   switch (res.type.value)
   {
   //普通结果
   case result_info::RESULT_TYPE_ID_NORMAL.value:
      __rand_normal_res(game, res, nums[1], nums[2]);
      break;

   //龙
   case result_info::RESULT_TYPE_ID_DRAGONL.value:
      __rand_dragon_res(game, res, nums[1], nums[2]);
      break;

   //骷髅
   case result_info::RESULT_TYPE_ID_SKELETON.value:
      res.multiple = 0;
      break;

   //打枪
   case result_info::RESULT_TYPE_ID_SHOOT.value:
      __random_shoot_res(game, res, nums[1], nums[2], nums + 3);
      break;

   //火车
   case result_info::RESULT_TYPE_ID_TRAIN.value:
      __random_train_res(game, res, nums[1], nums[2]);
      break;

   //多倍
   case result_info::RESULT_TYPE_ID_MULTIPLE.value:
      __random_multiple_res(game, res, nums[1], nums[2], nums[3]);
      break;

   //小四喜-兽族
   case result_info::RESULT_TYPE_ID_ORC.value:
      __random_orc_res(game, res);
      break;

   //小四喜-人族
   case result_info::RESULT_TYPE_ID_TERRAN.value:
      __random_humen_res(game, res);
      break;

   //大四喜-天使
   case result_info::RESULT_TYPE_ID_ANGEL.value:
      __random_angel_res(game, res);
      break;

   default:
      check(false, "unknown game type");
      break;
   }
}

template <typename Table, typename Item>
void WarGame::__rand_item(const name &scope, uint32_t total_weight, uint32_t rnum, Item &it)
{
   Table table(get_self(), scope.value);
   auto itr = table.begin();
   check(itr != table.end(), "no weights data");

   uint32_t hit_weight = rnum % total_weight;
   uint32_t cur_weight = 0;
   while (itr != table.end())
   {
      cur_weight += itr->weight;
      if (hit_weight < cur_weight)
      {
         it = *itr;
         break;
      }
      else
      {
         itr++;
      }
   }
}

void WarGame::__rand_res_type(const name &game, name &type, uint32_t rnum)
{
   result_type_weight it;
   __rand_item<restypeweis_table, result_type_weight>(
       game,
       game.value == WAR_LOW_GAME_NAME.value ? result_type_weight::LOW_TOTAL_WEIGHT : result_type_weight::HIGH_TOTAL_WEIGHT,
       rnum,
       it);
   type = it.type;
}

void WarGame::__rand_normal_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2)
{
   item_weight it;
   __rand_item<itemweis_table, item_weight>(
      game,
      item_weight::TOTAL_WEIGHT,
      rnum1,
      it
   );

   res.multiple = 0;
   res.items.push_back(
   {
      it.item.id,
      it.item.multiple,
      __rand_item_index(it.item.index, rnum2)}
   );
}

void WarGame::__rand_dragon_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2)
{
   //随机龙的倍数范围
   dragon_weights it;
   __rand_item<dragonweis_table, dragon_weights>(
      game,
      game.value == WAR_LOW_GAME_NAME.value ? dragon_weights::LOW_TOTAL_WEIGHT : dragon_weights::HIGH_TOTAL_WEIGHT,
      rnum1,
      it
   );

   //随机龙的倍数
   uint8_t multiple = uint8_t(it.min_multiple + rnum2 % (it.max_multiple - it.min_multiple + 1));

   res.multiple = 0;
   res.items.push_back(
   {
      item_info::ITEM_ID_DRAGON,
      multiple,
      13
   });
}

void WarGame::__rand_shoot_scope_and_weight(const name &game, uint32_t rnum, name &table_scope, uint32_t &total_weight)
{
   uint8_t count = __rand_2_3_4(rnum);
   if (game.value == WAR_LOW_GAME_NAME.value)
   {
      if (count == 2)
      {
         table_scope = items_weights::SHOOT_LOW_2;
         total_weight = items_weights::SHOOT_LOW_TOTAL_WEIGHT_2;
      }
      else if (count == 3)
      {
         table_scope = items_weights::SHOOT_LOW_3;
         total_weight = items_weights::SHOOT_LOW_TOTAL_WEIGHT_3;
      }
      else
      {
         table_scope = items_weights::SHOOT_LOW_4;
         total_weight = items_weights::SHOOT_LOW_TOTAL_WEIGHT_4;
      }
   }
   else
   {
      if (count == 2)
      {
         table_scope = items_weights::SHOOT_HIGH_2;
         total_weight = items_weights::SHOOT_HIGH_TOTAL_WEIGHT_2;
      }
      else if (count == 3)
      {
         table_scope = items_weights::SHOOT_HIGH_3;
         total_weight = items_weights::SHOOT_HIGH_TOTAL_WEIGHT_3;
      }
      else
      {
         table_scope = items_weights::SHOOT_HIGH_4;
         total_weight = items_weights::SHOOT_HIGH_TOTAL_WEIGHT_4;
      }
   }
}

void WarGame::__random_shoot_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2, uint32_t *rnums)
{
   res.multiple = 0;

   name table_scope;
   uint32_t total_weight;
   __rand_shoot_scope_and_weight(game, rnum1, table_scope, total_weight);

   items_weights it;
   __rand_item<shootweis_table, items_weights>(
      table_scope,
      total_weight,
      rnum2,
      it
   );

   uint8_t i = 0;
   vector<item_info>::const_iterator item_itr = it.items.begin();
   while (item_itr != it.items.end())
   {
      res.items.push_back(
      {
         item_itr->id,
         item_itr->multiple,
         __rand_item_index(item_itr->index, rnums[i])
      });

      item_itr++;
      i++;
   }
}

void WarGame::__rand_train_scope_and_weight(const name &game, uint32_t rnum, name &table_scope, uint32_t &total_weight)
{
   uint8_t count = __rand_2_3_4(rnum);
   if (game.value == WAR_LOW_GAME_NAME.value)
   {
      if (count == 2)
      {
         table_scope = items_weights::TRAIN_LOW_2;
         total_weight = items_weights::TRAIN_LOW_TOTAL_WEIGHT_2;
      }
      else if (count == 3)
      {
         table_scope = items_weights::TRAIN_LOW_3;
         total_weight = items_weights::TRAIN_LOW_TOTAL_WEIGHT_3;
      }
      else
      {
         table_scope = items_weights::TRAIN_LOW_4;
         total_weight = items_weights::TRAIN_LOW_TOTAL_WEIGHT_4;
      }
   }
   else
   {
      if (count == 2)
      {
         table_scope = items_weights::TRAIN_HIGH_2;
         total_weight = items_weights::TRAIN_HIGH_TOTAL_WEIGHT_2;
      }
      else if (count == 3)
      {
         table_scope = items_weights::TRAIN_HIGH_3;
         total_weight = items_weights::TRAIN_HIGH_TOTAL_WEIGHT_3;
      }
      else
      {
         table_scope = items_weights::TRAIN_HIGH_4;
         total_weight = items_weights::TRAIN_HIGH_TOTAL_WEIGHT_4;
      }
   }
}

void WarGame::__random_train_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2)
{
   name table_scope;
   uint32_t total_weight;
   __rand_train_scope_and_weight(game, rnum1, table_scope, total_weight);

   items_weights it;
   __rand_item<trainweis_table, items_weights>(
      table_scope,
      total_weight,
      rnum2,
      it);

   res.multiple = 0;
   vector<item_info>::const_iterator item_itr = it.items.begin();
   while (item_itr != it.items.end())
   {
      res.items.push_back(
      {
         item_itr->id,
         item_itr->multiple,
         item_itr->index
      });
      item_itr++;
   }
}

void WarGame::__random_multiple_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2, uint32_t rnum3)
{
   __rand_normal_res(game, res, rnum1, rnum2);

   uint8_t count = __rand_2_3_4(rnum3);
   res.multiple = count;
   res.items[0].multiple *= count;
}

void WarGame::__random_orc_res(const name &game, result_info &res)
{
   res.multiple = 0;
   if (game.value == WAR_LOW_GAME_NAME.value)
   {
      res.items.push_back({item_info::ITEM_ID_ORC_CHIEF, 12, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_TAUREN, 8, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_SHAMAN, 8, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_ORCS, 6, 0});
   }
   else
   {
      res.items.push_back({item_info::ITEM_ID_ORC_CHIEF, 24, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_TAUREN, 12, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_SHAMAN, 8, 0});
      res.items.push_back({item_info::ITEM_ID_ORC_ORCS, 4, 0});
   }
}

void WarGame::__random_humen_res(const name &game, result_info &res)
{
   res.multiple = 0;
   if (game.value == WAR_LOW_GAME_NAME.value)
   {
      res.items.push_back({item_info::ITEM_ID_TERRAN_KING, 12, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_KNIGHT, 8, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_MINISTER, 8, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_WARRIOR, 6, 0});
   }
   else
   {
      res.items.push_back({item_info::ITEM_ID_TERRAN_KING, 24, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_KNIGHT, 12, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_MINISTER, 8, 0});
      res.items.push_back({item_info::ITEM_ID_TERRAN_WARRIOR, 4, 0});
   }
}

void WarGame::__random_angel_res(const name &game, result_info &res)
{
   __random_orc_res(game, res);
   __random_humen_res(game, res);
}