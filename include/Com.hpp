#include <eosio/eosio.hpp>
#include <eosio/system.hpp>

using namespace eosio;

//#define __TEST_NET__

//定义合约账号
#ifdef __TEST_NET__
static constexpr name SERVER_ACCOUNT       {"hanstest1111"_n}; //游戏合约账号
static constexpr name BET_LOG_ACCOUNT      {"hanstest1111"_n}; //日志账号
static constexpr name CREATE_RANDOM_ACCOUNT{"hanstest1111"_n}; //提供随机数的账号
#else
static constexpr name SERVER_ACCOUNT       {"blockwar1234"_n}; //游戏合约账号
static constexpr name BET_LOG_ACCOUNT      {"blockwar1234"_n}; //日志账号
static constexpr name CREATE_RANDOM_ACCOUNT{"blockwarrand"_n}; //提供随机数的账号

#endif


