#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include "../../include/Com.hpp"

using namespace eosio;
using namespace std;

CONTRACT WarGame : public contract
{
public:
   using contract::contract;

   WarGame(const name &receiver, const name &code, const datastream<const char *> &ds);

   //定义游戏名称
   static constexpr name WAR_LOW_GAME_NAME {"low.war"_n }; //低倍战场
   static constexpr name WAR_HIGH_GAME_NAME{"high.war"_n}; //高倍战场

   //游戏物件信息
   struct item_info
   {
      static constexpr name ITEM_ID_TERRAN_KING    {"king"_n    }; //人族-国王
      static constexpr name ITEM_ID_TERRAN_KNIGHT  {"knight"_n  }; //人族-骑士
      static constexpr name ITEM_ID_TERRAN_MINISTER{"minister"_n}; //人族-牧师
      static constexpr name ITEM_ID_TERRAN_WARRIOR {"warrior"_n }; //人族-战士

      static constexpr name ITEM_ID_ORC_CHIEF {"chief"_n }; //兽族-酋长
      static constexpr name ITEM_ID_ORC_TAUREN{"tauren"_n}; //兽族-牛头人
      static constexpr name ITEM_ID_ORC_SHAMAN{"shaman"_n}; //兽族-萨满
      static constexpr name ITEM_ID_ORC_ORCS  {"orcs"_n  }; //兽族-兽人

      static constexpr name ITEM_ID_DRAGON{"gragon"_n}; //龙

      name id;          //角色类型
      uint8_t multiple; //倍数
      uint8_t index;    //地图位置

      EOSLIB_SERIALIZE(item_info, (id)(multiple)(index))
   };

   //游戏结果
   struct result_info
   {
      static constexpr name RESULT_TYPE_ID_NORMAL  {"normal"_n  }; //普通结果
      static constexpr name RESULT_TYPE_ID_DRAGONL {"dragon"_n  }; //龙
      static constexpr name RESULT_TYPE_ID_SKELETON{"skeleton"_n}; //骷髅
      static constexpr name RESULT_TYPE_ID_SHOOT   {"shoot"_n   }; //打枪
      static constexpr name RESULT_TYPE_ID_TRAIN   {"train"_n   }; //火车
      static constexpr name RESULT_TYPE_ID_MULTIPLE{"multiple"_n}; //多倍
      static constexpr name RESULT_TYPE_ID_ORC     {"orc"_n     }; //小四喜-兽族
      static constexpr name RESULT_TYPE_ID_TERRAN  {"terran"_n  }; //小四喜-人族
      static constexpr name RESULT_TYPE_ID_ANGEL   {"angel"_n   }; //大四喜-天使

      name type;               //游戏结果类型
      vector<item_info> items; //命中的角色，可能多个结果，也可能没有
      uint8_t multiple;        //多倍特效的倍数

      EOSLIB_SERIALIZE(result_info, (type)(items)(multiple))
   };

   //游戏信息
   struct game_info
   {
      name game;         //游戏名
      uint32_t match_id; //局id
      uint32_t round_id; //轮id

      EOSLIB_SERIALIZE(game_info, (game)(match_id)(round_id))
   };

   //初始化游戏结果样本权重
   ACTION initweights();

   //根据时间清空，每条数据保存一天
   ACTION clearress(const name &game);
   using clearress_action = action_wrapper<"clearress"_n, &WarGame::clearress>;

   ACTION clearrnums(const string &memo);
   using clearrnums_action = action_wrapper<"clearrnums"_n, &WarGame::clearrnums>;

   /**
    * 测试，根据随机数生成游戏结果，任何人都可以调用，会生成日志，但是不保存
    *
    * @param actor - 调用者
    * @param game - 游戏名
    * @param rand_value - 随机数
    */
   ACTION randtest(const name &actor, const name &game, const checksum256 &rand_value);

   //测试日志
   ACTION randtestlog(const name &actor, const name &game, const checksum256 &rand_value, const result_info &res);
   using randtestlog_action = action_wrapper<"randtestlog"_n, &WarGame::randtestlog>;

   /**
    * 请求生产随机结果，只有服务器账号才能调用，游戏id不能重复
    *
    * @param game - 游戏信息
    * @param hash_seed - 随机数种子的hash值，hash_seed=sha256(seed)
    */
   ACTION requesttask(const game_info &game, const checksum256 &hash_seed);
   
   /**
    * 生产游戏结果，只有服务器账号才能调用
    *
    * @param game - 游戏信息
    * @param seed - 随机数种子
    */
   ACTION dotask(const game_info &game, const string &seed);

   //日志
   ACTION dotasklog(const game_info &game, const checksum256 &rand_value, const result_info &res);
   using dotasklog_action = action_wrapper<"dotasklog"_n, &WarGame::dotasklog>;

   //下注记录
   struct bet_item
   {
      name     item_id; //物件id
      uint64_t amount;  //下注的数量

      EOSLIB_SERIALIZE(bet_item, (item_id)(amount))
   };

   //用户下注记录
   struct user_bet_items
   {
      uint64_t uid;           //用户id
      vector<bet_item> items; //下注记录

      EOSLIB_SERIALIZE(user_bet_items, (uid)(items))
   };

   /**
    * 下注日志，只有服务器账号才能调用
    *
    * @param game - 游戏信息
    * @param bets - 下注记录
    */
   ACTION betlog(const game_info &game, vector<user_bet_items> &bets);

   /**
    * 接收随机数
    *
    * @param assoc_id - task_id
    * @param random_value - 生产的随机数
    */
   ACTION receiverand(uint64_t assoc_id, const checksum256 &random_value);

private:
   //单例数据
   TABLE sendids
   {
      uint32_t id;
   };
   using sendids_singleton = singleton<"sendids"_n, sendids>;
   sendids_singleton __sendids;

   //singleton命名应该有点问题，不得已定义两个一个的table
   TABLE taskids
   {
      uint32_t id;
   };
   using taskids_singleton = singleton<"taskids"_n, sendids>;
   taskids_singleton __taskids;

   //请求随机数任务，此表任务完成后数据会自动删除
   TABLE task_request_rand
   {  
      uint64_t    id;     //任务id
      game_info   game;   //可选参数-随机游戏结果，游戏信息
      checksum256 hash_seed; //种子的hash值
      time_point  ctime;  //数据创建时间
      
      uint64_t primary_key() const { return id; }

      uint64_t by_time() const { return ctime.time_since_epoch().count(); }

      EOSLIB_SERIALIZE(task_request_rand, (id)(game)(hash_seed)(ctime))
   };
   using randtasks_table = multi_index<
      "rntasks"_n, 
      task_request_rand,
      indexed_by<"bytime"_n, const_mem_fun<task_request_rand, uint64_t, &task_request_rand::by_time>>
   >;
   randtasks_table __tasks;

   //创建游戏结果任务，此表数据需要调用delresult才能删除
   TABLE task_create_result
   {
      game_info   game;    //游戏信息
      uint64_t    task_id; //任务id
      checksum256 rnum;    //随机数
      result_info result;  //游戏结果
      time_point  ctime;   //数据创建时间

      uint64_t primary_key() const 
      {
         uint64_t key = game.match_id;
         key <<= 32;
         key |= game.round_id;
         return key;
      }

      uint64_t by_time() const { return ctime.time_since_epoch().count(); }

      EOSLIB_SERIALIZE(task_create_result, (game)(task_id)(rnum)(result)(ctime))
   };
   using resulttasks_table = multi_index<
      "retasks"_n, 
      task_create_result,
      indexed_by<"bytime"_n, const_mem_fun<task_create_result, uint64_t, &task_create_result::by_time>>
   >;

   //游戏结果类型权重表
   TABLE result_type_weight
   {
      //总权重
      static constexpr uint32_t LOW_TOTAL_WEIGHT  = 1230060;
      static constexpr uint32_t HIGH_TOTAL_WEIGHT = 3623280;

      name type;       //名称，方便查看日志
      uint32_t weight; //权重

      uint64_t primary_key() const { return type.value; }

      EOSLIB_SERIALIZE(result_type_weight, (type)(weight))
   };
   using restypeweis_table = multi_index<"restypeweis"_n, result_type_weight>;

   // item权重信息
   struct item_weight_info
   {
      item_info item;  //名称，方便查看日志
      uint32_t weight; //权重

      EOSLIB_SERIALIZE(item_weight_info, (item)(weight))
   };

   //游戏角色权重
   TABLE item_weight
   {
      //总权重
      static constexpr uint32_t TOTAL_WEIGHT = 24;

      item_info item;  //名称，方便查看日志
      uint32_t weight; //权重

      uint64_t primary_key() const { return item.id.value; }

      EOSLIB_SERIALIZE(item_weight, (item)(weight))
   };
   using itemweis_table = multi_index<"itemweis"_n, item_weight>;

   //龙的倍数分布权重
   TABLE dragon_weights
   {
      //总权重
      static constexpr uint32_t LOW_TOTAL_WEIGHT  = 64;
      static constexpr uint32_t HIGH_TOTAL_WEIGHT = 125;

      uint8_t min_multiple; //最小倍数
      uint8_t max_multiple; //最大倍数
      uint32_t weight;      //权重

      uint64_t primary_key() const { return min_multiple; }

      EOSLIB_SERIALIZE(dragon_weights, (min_multiple)(max_multiple)(weight))
   };
   using dragonweis_table = multi_index<"dragonweis"_n, dragon_weights>;

   //打枪结果样本权重关系表
   TABLE items_weights
   {
      //低倍打枪
      static constexpr name SHOOT_LOW_2{"shoot.low.2"_n}; //低倍2枪
      static constexpr name SHOOT_LOW_3{"shoot.low.3"_n}; //低倍3枪
      static constexpr name SHOOT_LOW_4{"shoot.low.4"_n}; //低倍4枪

      static constexpr uint32_t SHOOT_LOW_TOTAL_WEIGHT_2 = 168;
      static constexpr uint32_t SHOOT_LOW_TOTAL_WEIGHT_3 = 504;
      static constexpr uint32_t SHOOT_LOW_TOTAL_WEIGHT_4 = 840;

      //高倍打枪
      static constexpr name SHOOT_HIGH_2{"shoot.high.2"_n}; //高倍2枪
      static constexpr name SHOOT_HIGH_3{"shoot.high.3"_n}; //高倍3枪
      static constexpr name SHOOT_HIGH_4{"shoot.high.4"_n}; //高倍4枪

      static constexpr uint32_t SHOOT_HIGH_TOTAL_WEIGHT_2 = 168;
      static constexpr uint32_t SHOOT_HIGH_TOTAL_WEIGHT_3 = 504;
      static constexpr uint32_t SHOOT_HIGH_TOTAL_WEIGHT_4 = 840;

      //低倍火车
      static constexpr name TRAIN_LOW_2{"train.low.2"_n}; //低倍2节车厢
      static constexpr name TRAIN_LOW_3{"train.low.3"_n}; //低倍3节车厢
      static constexpr name TRAIN_LOW_4{"train.low.4"_n}; //低倍4节车厢

      static constexpr uint32_t TRAIN_LOW_TOTAL_WEIGHT_2 = 132;
      static constexpr uint32_t TRAIN_LOW_TOTAL_WEIGHT_3 = 180;
      static constexpr uint32_t TRAIN_LOW_TOTAL_WEIGHT_4 = 216;

      //高倍火车
      static constexpr name TRAIN_HIGH_2{"train.high.2"_n}; //高倍2节车厢
      static constexpr name TRAIN_HIGH_3{"train.high.3"_n}; //高倍3节车厢
      static constexpr name TRAIN_HIGH_4{"train.high.4"_n}; //高倍4节车厢

      static constexpr uint32_t TRAIN_HIGH_TOTAL_WEIGHT_2 = 130;
      static constexpr uint32_t TRAIN_HIGH_TOTAL_WEIGHT_3 = 178;
      static constexpr uint32_t TRAIN_HIGH_TOTAL_WEIGHT_4 = 216;

      uint16_t id;
      vector<item_info> items; //命中的角色，可能多个结果，也可能没有
      uint32_t weight;

      uint64_t primary_key() const { return id; }

      EOSLIB_SERIALIZE(items_weights, (id)(items)(weight))
   };
   using shootweis_table = multi_index<"shootweis"_n, items_weights>;
   using trainweis_table = multi_index<"trainweis"_n, items_weights>;

private:
   //延迟调用id
   uint32_t __create_sender_id();
   uint32_t __create_task_id();

   //延迟执行交易
   template <typename Action, typename... Args>
   void __send_transaction(const name &payer, Args&&... args);

   //获取游戏id
   uint64_t __get_game_id(const game_info &game);

   //结果类型权重表
   void __add_to_type_weights(restypeweis_table &table, const name &type, uint32_t weight);
   void __init_type_weights();
   
   //item权重表
   void __add_to_item_weights(itemweis_table &table, const name &id, uint32_t weight, uint8_t multiple, uint8_t index);
   void __init_item_weights();
   
   //龙的权重分布
   void __add_to_dragon_weights(dragonweis_table &table, uint8_t min_multiple, uint8_t max_multiple, uint32_t weight);
   void __init_dragon_weights();

   //打枪样本权重分布
   void __add_to_shoot_weights_2(shootweis_table &table, item_weight_info &item1, item_weight_info &item2);
   void __add_to_shoot_weights_3(shootweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3);
   void __add_to_shoot_weights_4(shootweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3, item_weight_info &item4);
   void __init_shoot_weights();

   //火车样本权重分布
   void __add_to_train_weights_2(trainweis_table &table, item_weight_info &item1, item_weight_info &item2);
   void __add_to_train_weights_3(trainweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3);
   void __add_to_train_weights_4(trainweis_table &table, item_weight_info &item1, item_weight_info &item2, item_weight_info &item3, item_weight_info &item4);
   void __init_train_weights();

   //随机数处理
   uint32_t __create_uint32(uint8_t num1, uint8_t num2, uint8_t num3, uint8_t num4);
   void __checksum256_to_uint32s(const checksum256 &rand_value, uint32_t *nums);

   //随机2-3-4
   uint8_t __rand_2_3_4(uint32_t rnum);

   //随机item下标
   uint8_t __rand_item_index(uint8_t min_index, uint32_t rnum);

   //随机出样本
   template<typename Table, typename Item>
   void __rand_item(const name &scope, uint32_t total_weight, uint32_t rnum, Item &it);

   //随机游戏结果
   void __rand_res(const name &game, result_info &res, const checksum256 &rand_value);

   void __rand_res_type(const name &game, name &type, uint32_t rnum);
   void __rand_normal_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2);
   void __rand_dragon_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2);

   //打枪
   void __rand_shoot_scope_and_weight(const name &game, uint32_t rnum, name &table_scope, uint32_t &total_weight);
   void __random_shoot_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2, uint32_t *rnums);

   //火车
   void __rand_train_scope_and_weight(const name &game, uint32_t rnum, name &table_scope, uint32_t &total_weight);
   void __random_train_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2);

   void __random_multiple_res(const name &game, result_info &res, uint32_t rnum1, uint32_t rnum2, uint32_t rnum3);

   void __random_orc_res(const name &game, result_info &res);
   void __random_humen_res(const name &game, result_info &res);
   void __random_angel_res(const name &game, result_info &res);
};