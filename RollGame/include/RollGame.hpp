#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include "../../include/Com.hpp"

using namespace eosio;
using namespace std;

CONTRACT RollGame : public contract
{
public:
   using contract::contract;

   RollGame(const name &receiver, const name &code, const datastream<const char *> &ds);

   /**
    * 测试，根据随机数生成结果，任何人都可以调用，会生成日志，但是不保存
    *
    * @param actor - 调用者
    * @param rand_value - 随机数
    */
   ACTION randtest(const name &actor, const checksum256 &rand_value);

   //测试日志
   ACTION randtestlog(const name &actor, const checksum256 &rand_value, uint8_t res);
   using randtestlog_action = action_wrapper<"randtestlog"_n, &RollGame::randtestlog>;

   /**
    * 请求生成随机结果，只有服务器账号才能调用
    *
    * @param id - 请求id，不能重复
    * @param hash_seed - 随机数种子的hash值，hash_seed=sha256(seed)
    */
   ACTION requesttask(uint64_t id, const checksum256 &hash_seed);
   
   /**
    * 生产游戏结果，只有服务器账号才能调用
    *
    * @param id - 请求id，不能重复
    * @param seed - 种子
    */
   ACTION dotask(uint64_t id, const string &seed);

   //日志
   ACTION dotasklog(uint64_t id, const checksum256 &rand_value, uint8_t res);
   using dotasklog_action = action_wrapper<"dotasklog"_n, &RollGame::dotasklog>;

   //下注记录
   struct bet_item
   {
      //押大小
      static constexpr name ITEM_ID_BIG   {"big"_n};   
      static constexpr name ITEM_ID_SMALL {"small"_n};

      //押数字，不用阿拉伯数字是因为name不能包含‘6’
      static constexpr name ITEM_ID_NUMBER_1 {"num.one"_n};
      static constexpr name ITEM_ID_NUMBER_2 {"num.two"_n};
      static constexpr name ITEM_ID_NUMBER_3 {"num.thress"_n};
      static constexpr name ITEM_ID_NUMBER_4 {"num.four"_n};
      static constexpr name ITEM_ID_NUMBER_5 {"num.five"_n};
      static constexpr name ITEM_ID_NUMBER_6 {"num.six"_n};

      name     item_id; //物件id
      uint64_t amount;  //下注的数量

      EOSLIB_SERIALIZE(bet_item, (item_id)(amount))
   };

   /**
    * 下注日志，只有服务器账号才能调用
    *
    * @param gid - 游戏id
    * @param uid - 用户id
    * @param bets - 下注记录
    */
   ACTION betlog(uint64_t gid, uint64_t uid, vector<bet_item> &bets);

   //根据时间清空，每条数据保存一天
   ACTION clearress(const string &memo);
   using clearress_action = action_wrapper<"clearress"_n, &RollGame::clearress>;

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

   //请求随机数任务，此表任务完成后数据会自动删除
   TABLE task
   {  
      uint64_t    id;        //任务id
      checksum256 hash_seed; //种子的hash值
      checksum256 rnum;      //随机数
      uint8_t     res;       //结果：1-6
      time_point  ctime;     //数据创建时间
      
      uint64_t primary_key() const { return id; }

      uint64_t by_time() const { return ctime.time_since_epoch().count(); }

      EOSLIB_SERIALIZE(task, (id)(hash_seed)(rnum)(res)(ctime))
   };
   using tasks_table = multi_index<"rtasks"_n, task, indexed_by<"bytime"_n, const_mem_fun<task, uint64_t, &task::by_time>>>;
   tasks_table __tasks;

private:
   //延迟调用id
   uint32_t __create_sender_id();

   //延迟执行交易
   template <typename Action, typename... Args>
   void __send_transaction(const name &payer, Args&&... args);

   //随机数处理
   uint8_t __rand_res(const checksum256 &rand_value);
};