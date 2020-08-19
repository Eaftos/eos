#include <algorithm>
#include <vector>
#include <iterator>
#include <cstdlib>
#include <sstream>

#include <boost/test/unit_test.hpp>

#include <fc/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/scoped_exit.hpp>

#include <eosio/chain/contract_types.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/testing/tester.hpp>

#include <boost/test/framework.hpp>

#include <deep_nested.abi.hpp>
#include <large_nested.abi.hpp>

using namespace eosio;
using namespace chain;



BOOST_AUTO_TEST_SUITE(abi_tests)

fc::microseconds max_serialization_time = fc::seconds(1); // some test machines are very slow

// verify that round trip conversion, via bytes, reproduces the exact same data
fc::variant verify_byte_round_trip_conversion( const abi_serializer& abis, const type_name& type, const fc::variant& var )
{
   auto bytes = abis.variant_to_binary(type, var, abi_serializer::create_yield_function( max_serialization_time ));

   auto var2 = abis.binary_to_variant(type, bytes, abi_serializer::create_yield_function( max_serialization_time ));

   std::string r = fc::json::to_string(var2, fc::time_point::now() + max_serialization_time);

   auto bytes2 = abis.variant_to_binary(type, var2, abi_serializer::create_yield_function( max_serialization_time ));

   BOOST_TEST( fc::to_hex(bytes) == fc::to_hex(bytes2) );

   return var2;
}

void verify_round_trip_conversion( const abi_serializer& abis, const type_name& type, const std::string& json, const std::string& hex, const std::string& expected_json )
{
   auto var = fc::json::from_string(json);
   auto bytes = abis.variant_to_binary(type, var, abi_serializer::create_yield_function( max_serialization_time ));
   BOOST_REQUIRE_EQUAL(fc::to_hex(bytes), hex);
   auto var2 = abis.binary_to_variant(type, bytes, abi_serializer::create_yield_function( max_serialization_time ));
   BOOST_REQUIRE_EQUAL(fc::json::to_string(var2, fc::time_point::now() + max_serialization_time), expected_json);
   auto bytes2 = abis.variant_to_binary(type, var2, abi_serializer::create_yield_function( max_serialization_time ));
   BOOST_REQUIRE_EQUAL(fc::to_hex(bytes2), hex);
}

void verify_round_trip_conversion( const abi_serializer& abis, const type_name& type, const std::string& json, const std::string& hex )
{
   verify_round_trip_conversion( abis, type, json, hex, json );
}

auto get_resolver(const abi_def& abi = abi_def())
{
   return [&abi](const account_name &name) -> optional<abi_serializer> {
      return abi_serializer(eosio_contract_abi(abi), abi_serializer::create_yield_function( max_serialization_time ));
   };
}

// verify that round trip conversion, via actual class, reproduces the exact same data
template<typename T>
fc::variant verify_type_round_trip_conversion( const abi_serializer& abis, const type_name& type, const fc::variant& var )
{ try {

   auto bytes = abis.variant_to_binary(type, var, abi_serializer::create_yield_function( max_serialization_time ));

   T obj;
   abi_serializer::from_variant(var, obj, get_resolver(), abi_serializer::create_yield_function( max_serialization_time ));

   fc::variant var2;
   abi_serializer::to_variant(obj, var2, get_resolver(), abi_serializer::create_yield_function( max_serialization_time ));

   std::string r = fc::json::to_string(var2, fc::time_point::now() + max_serialization_time);


   auto bytes2 = abis.variant_to_binary(type, var2, abi_serializer::create_yield_function( max_serialization_time ));

   BOOST_TEST( fc::to_hex(bytes) == fc::to_hex(bytes2) );

   return var2;
} FC_LOG_AND_RETHROW() }




BOOST_AUTO_TEST_CASE(setabi_test)
{
   try {
   const string abi_string = R"=====(
      {
        "version": "eosio::abi/1.0",
        "types": [{
            "new_type_name": "account_name",
            "type": "name"
          }
        ],
        "structs": [],
        "actions": [],
        "tables": [],
        "kv_tables": {
            "kvtable1": {
                "type": "kvaccount1",
                "primary_index": {"name": "pida", "type": "name"},
                "secondary_indices": {
                    "sid1": {"type": "string"},
                    "sid2": {"type": "uint32"},
                    "sid3": {"type": "name"}
                }
            },
            "kvtable2": {
                "type": "kvaccount2",
                "primary_index": {"name": "pidb", "type": "name"},
                "secondary_indices": {
                    "sida": {"type": "int32"},
                    "sidb": {"type": "uint64"},
                    "sidc": {"type": "sha256"}
                }
            }
        },
       "ricardian_clauses": [],
       "abi_extensions": []
      }
   )=====";

   auto var = fc::json::from_string(abi_string.c_str());
   auto abi = var.as<abi_def>();
   fc::variant v1;
   fc::variant v2;
   kv_tables_as_object<map<table_name, kv_table_def>> kv_tables_obj;

   to_variant(abi.kv_tables, v1);
   from_variant(v1, kv_tables_obj);
   to_variant(kv_tables_obj, v2);

   std::stringstream ss1;
   std::stringstream ss2;
   ss1 << v1;
   ss2 << v2;
   string str1 = ss1.str();
   string str2 = ss2.str();

   BOOST_TEST(str1 == str2);
   BOOST_TEST(2u == abi.kv_tables.value.size());
   name tbl_name = name("kvtable1");
   BOOST_TEST("pida" == abi.kv_tables.value[tbl_name].primary_index.name.to_string());
   BOOST_TEST("name" == abi.kv_tables.value[tbl_name].primary_index.type);
   BOOST_TEST(3u == abi.kv_tables.value[tbl_name].secondary_indices.size());
   BOOST_TEST("string" == abi.kv_tables.value[tbl_name].secondary_indices[name("sid1")].type);
   BOOST_TEST("uint32" == abi.kv_tables.value[tbl_name].secondary_indices[name("sid2")].type);
   BOOST_TEST("name" == abi.kv_tables.value[tbl_name].secondary_indices[name("sid3")].type);

   tbl_name = name("kvtable2");
   BOOST_TEST("pidb" == abi.kv_tables.value[tbl_name].primary_index.name.to_string());
   BOOST_TEST("name" == abi.kv_tables.value[tbl_name].primary_index.type);
   BOOST_TEST_REQUIRE(3u == abi.kv_tables.value[tbl_name].secondary_indices.size());
   BOOST_TEST("int32" == abi.kv_tables.value[tbl_name].secondary_indices[name("sida")].type);
   BOOST_TEST("uint64" == abi.kv_tables.value[tbl_name].secondary_indices[name("sidb")].type);
   BOOST_TEST("sha256" == abi.kv_tables.value[tbl_name].secondary_indices[name("sidc")].type);

    std::cout << "*********** to_variant tested success" << std::endl;

   abi_serializer abis(abi, abi_serializer::create_yield_function(max_serialization_time));
   auto var2 = verify_byte_round_trip_conversion( abis, "abi_def", var );
   auto abi2 = var2.as<abi_def>();



   }
   FC_LOG_AND_RETHROW()
   }

struct action1 {
   action1() = default;
   action1(uint64_t b1, uint32_t b2, uint8_t b3) : blah1(b1), blah2(b2), blah3(b3) {}
   uint64_t blah1;
   uint32_t blah2;
   uint8_t blah3;
   static account_name get_account() { return N(acount1); }
   static account_name get_name() { return N(action1); }

   template<typename Stream>
   friend Stream& operator<<( Stream& ds, const action1& act ) {
     ds << act.blah1 << act.blah2 << act.blah3;
     return ds;
   }

   template<typename Stream>
   friend Stream& operator>>( Stream& ds, action1& act ) {
      ds >> act.blah1 >> act.blah2 >> act.blah3;
     return ds;
   }
};

struct action2 {
   action2() = default;
   action2(uint32_t b1, uint64_t b2, uint8_t b3) : blah1(b1), blah2(b2), blah3(b3) {}
   uint32_t blah1;
   uint64_t blah2;
   uint8_t blah3;
   static account_name get_account() { return N(acount2); }
   static account_name get_name() { return N(action2); }

   template<typename Stream>
   friend Stream& operator<<( Stream& ds, const action2& act ) {
     ds << act.blah1 << act.blah2 << act.blah3;
     return ds;
   }

   template<typename Stream>
   friend Stream& operator>>( Stream& ds, action2& act ) {
      ds >> act.blah1 >> act.blah2 >> act.blah3;
     return ds;
   }
};

template<typename T>
void verify_action_equal(const chain::action& exp, const chain::action& act)
{
   BOOST_REQUIRE_EQUAL(exp.account.to_string(), act.account.to_string());
   BOOST_REQUIRE_EQUAL(exp.name.to_string(), act.name.to_string());
   BOOST_REQUIRE_EQUAL(exp.authorization.size(), act.authorization.size());
   for(unsigned int i = 0; i < exp.authorization.size(); ++i)
   {
      BOOST_REQUIRE_EQUAL(exp.authorization[i].actor.to_string(), act.authorization[i].actor.to_string());
      BOOST_REQUIRE_EQUAL(exp.authorization[i].permission.to_string(), act.authorization[i].permission.to_string());
   }
   BOOST_REQUIRE_EQUAL(exp.data.size(), act.data.size());
   BOOST_REQUIRE(!memcmp(exp.data.data(), act.data.data(), exp.data.size()));
}

private_key_type get_private_key( name keyname, string role ) {
   return private_key_type::regenerate<fc::ecc::private_key_shim>(fc::sha256::hash(keyname.to_string()+role));
}

public_key_type  get_public_key( name keyname, string role ) {
   return get_private_key( keyname, role ).get_public_key();
}


BOOST_AUTO_TEST_SUITE_END()
