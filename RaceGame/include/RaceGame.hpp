#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include "../../include/Com.hpp"

using namespace eosio;
using namespace std;

CONTRACT RaceGame : public contract
{
public:
   using contract::contract;

   RaceGame(const name &receiver, const name &code, const datastream<const char *> &ds);

   //游戏物件信息
   struct item_info
   {
      static constexpr name ITEM_ID_LION    {"lion"_n   }; //狮子
      static constexpr name ITEM_ID_ALPACA  {"alpaca"_n }; //草泥马
      static constexpr name ITEM_ID_HORSE   {"horse"_n  }; //马
      static constexpr name ITEM_ID_OSTRICH {"ostrich"_n}; //鸵鸟
      static constexpr name ITEM_ID_BOAR    {"boar"_n   }; //野猪
      static constexpr name ITEM_ID_COW     {"cow"_n    }; //奶牛
      static constexpr name ITEM_ID_PHOENIX {"phoenix"_n}; //凤凰
      
      name     id;       //角色类型
      uint32_t item_id;  //id，服务器分配的id
      uint8_t  multiple; //倍数
      uint8_t  level;    //等级

      EOSLIB_SERIALIZE(item_info, (id)(item_id)(multiple)(level))
   };

   //游戏结果
   struct result_info
   {
      static constexpr name RESULT_TYPE_ID_NORMAL  {"normal"_n  }; //普通结果
      static constexpr name RESULT_TYPE_ID_PHOENIX {"phoenix"_n }; //凤凰特效
      static constexpr name RESULT_TYPE_ID_SEVERAL {"several"_n }; //多个动物并列第一
      static constexpr name RESULT_TYPE_ID_MULTIPLE{"multiple"_n}; //多倍特效
      static constexpr name RESULT_TYPE_ID_DEATH   {"death"_n   }; //团灭特效
      static constexpr name RESULT_TYPE_ID_ALLWIN  {"allwin"_n  }; //全赢特效

      //类型定义权重
      static constexpr uint32_t WEIGHT_TYPE_NORMAL   = 390; //普通结果，只有参与赛跑的动物中有凤凰才会出现凤凰特效
      static constexpr uint32_t WEIGHT_TYPE_PHOENIX  = 20; //凤凰特效
      static constexpr uint32_t WEIGHT_TYPE_SEVERAL  = 20; //多个动物并列第一
      static constexpr uint32_t WEIGHT_TYPE_MULTIPLE = 20; //多倍特效
      static constexpr uint32_t WEIGHT_TYPE_DEATH    = 10; //团灭特效
      static constexpr uint32_t WEIGHT_TYPE_ALLWIN   = 8;  //全赢特效

      static constexpr uint32_t WEIGHT_TOTAL = 486; //总权重
      
      name type;               //游戏结果类型
      vector<item_info> items; //命中的角色，可能多个结果，也可能没有
      uint8_t multiple;        //多倍的倍数
      
      EOSLIB_SERIALIZE(result_info, (type)(items)(multiple))
   };

   /**
    * 测试，根据随机数生成游戏结果，任何人都可以调用，会生成日志，但是不保存
    *
    * @param actor - 调用者
    * @param runners - 参与赛跑的动物列表
    * @param rand_value - 随机数
    */
   ACTION randtest(const name &actor, const vector<item_info> &runners, const checksum256 &rand_value);

   //测试日志
   ACTION randtestlog(const name &actor, const vector<item_info> &runners, const checksum256 &rand_value, const result_info &res);
   using randtestlog_action = action_wrapper<"randtestlog"_n, &RaceGame::randtestlog>;

   /**
    * 请求生产随机结果，只有服务器账号才能调用，游戏id不能重复
    *
    * @param match_id - 轮id
    * @param round_id - 局id
    * @param hash_seed - 随机数种子的hash值，hash_seed=sha256(seed)
    */
   ACTION requesttask(uint32_t match_id, uint32_t round_id, const vector<item_info> &runners, const checksum256 &hash_seed);
   
   /**
    * 生产游戏结果，只有服务器账号才能调用
    *
    * @param match_id - 轮id
    * @param round_id - 局id
    * @param seed - 种子
    */
   ACTION dotask(uint32_t match_id, uint32_t round_id, const string &seed);

   //日志
   ACTION dotasklog(uint32_t match_id, uint32_t round_id, const vector<item_info> &runners, const checksum256 &rand_value, const result_info &res);
   using dotasklog_action = action_wrapper<"dotasklog"_n, &RaceGame::dotasklog>;

   //根据时间清空，每条数据保存一天
   ACTION clearress(const string &memo);
   using clearress_action = action_wrapper<"clearress"_n, &RaceGame::clearress>;

   //下注记录
   struct bet_item
   {
      name     item_id;  //物件id
      uint64_t amount;   //下注的数量
      uint8_t  multiple; //倍数

      EOSLIB_SERIALIZE(bet_item, (item_id)(amount)(multiple))
   };

   //用户下注记录
   struct user_bet_items
   {
      uint64_t uid;           //用户id
      vector<bet_item> items; //下注信息

      EOSLIB_SERIALIZE(user_bet_items, (uid)(items))
   };

   /**
    * 下注日志，只有服务器账号才能调用
    *
    * @param match_id - 轮id
    * @param round_id - 局id
    * @param bets - 下注记录
    */
   ACTION betlog(uint32_t match_id, uint32_t round_id, vector<user_bet_items> &bets);

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
      uint32_t          match_id;  //轮id
      uint32_t          round_id;  //局id
      vector<item_info> runners;   //参与赛跑的动物
      checksum256       hash_seed; //种子的hash值
      checksum256       rnum;      //随机数
      result_info       result;    //游戏结果
      time_point        ctime;     //数据创建时间

      uint64_t primary_key() const
      {
         uint64_t key = match_id;
         key <<= 32;
         key |= round_id;
         return key;
      }

      uint64_t by_time() const { return ctime.time_since_epoch().count(); }

      EOSLIB_SERIALIZE(task, (match_id)(round_id)(runners)(hash_seed)(rnum)(result)(ctime))
   };
   using tasks_table = multi_index<"ratasks"_n, task, indexed_by<"bytime"_n, const_mem_fun<task, uint64_t, &task::by_time>>>;
   tasks_table __tasks;

   //权重
   struct item_weight
   {
      item_info runner;
      uint32_t weight;

      EOSLIB_SERIALIZE(item_weight, (runner)(weight))
   };
   
private:
   //延迟调用id
   uint32_t __create_sender_id();

   //延迟执行交易
   template <typename Action, typename... Args>
   void __send_transaction(const name &payer, Args&&... args);

   //获取游戏id
   uint64_t __get_game_id(uint32_t match_id, uint32_t round_id);

   //随机数处理
   uint32_t __create_uint32(uint8_t num1, uint8_t num2, uint8_t num3, uint8_t num4);
   void __checksum256_to_uint32s(const checksum256 &rand_value, uint32_t *nums);

   //随机2-3-4
   uint8_t __rand_2_3_4(uint32_t rnum);

   //随机游戏结果
   void __rand_res(const vector<item_info> &runners, const checksum256 &rand_value, result_info &res);

   //判断runners中是否包含凤凰，如果有则去掉凤凰
   void __parse_runner(const vector<item_info> &runners, uint8_t &phoenix_count, item_info &phoenix_item, uint32_t &total_wei, vector<item_weight> &item_weis);

   //随机游戏特效
   void __rand_res_type(bool contain_phoenix, name &type, uint32_t rnum);
   void __rand_items(vector<item_weight> &item_weis, uint8_t count, uint32_t total_wei, vector<item_info> &items, uint32_t *nums);
};