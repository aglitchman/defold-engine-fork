#include <gtest/gtest.h>

#include "script.h"
#include "test/test_ddf.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/time.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PATH_FORMAT "build/default/src/test/%s"

#define DEFAULT_URL "__default_url"

dmhash_t ResolvePathCallback(uintptr_t user_data, const char* path, uint32_t path_size)
{
    return dmHashBuffer64(path, path_size);
}

void GetURLCallback(lua_State* L, dmMessage::URL* url)
{
    lua_getglobal(L, DEFAULT_URL);
    *url = *dmScript::CheckURL(L, -1);
    lua_pop(L, 1);
}

uintptr_t GetUserDataCallback(lua_State* L)
{
    lua_getglobal(L, DEFAULT_URL);
    uintptr_t default_url = (uintptr_t)dmScript::CheckURL(L, -1);
    lua_pop(L, 1);
    return default_url;
}

class ScriptMsgTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        L = lua_open();
        luaL_openlibs(L);
        m_ScriptContext = dmScript::NewContext(0, 0);
        dmScript::ScriptParams params;
        params.m_Context = m_ScriptContext;
        params.m_ResolvePathCallback = ResolvePathCallback;
        params.m_GetURLCallback = GetURLCallback;
        params.m_GetUserDataCallback = GetUserDataCallback;
        dmScript::Initialize(L, params);

        assert(dmMessage::NewSocket("default_socket", &m_DefaultURL.m_Socket) == dmMessage::RESULT_OK);
        m_DefaultURL.m_Path = dmHashString64("default_path");
        m_DefaultURL.m_Fragment = dmHashString64("default_fragment");
        dmScript::PushURL(L, m_DefaultURL);
        lua_setglobal(L, DEFAULT_URL);
    }

    virtual void TearDown()
    {
        dmMessage::DeleteSocket(m_DefaultURL.m_Socket);
        dmScript::Finalize(L, m_ScriptContext);
        lua_close(L);
        dmScript::DeleteContext(m_ScriptContext);
    }

    dmScript::HContext m_ScriptContext;
    lua_State* L;
    dmMessage::URL m_DefaultURL;
};

bool RunFile(lua_State* L, const char* filename)
{
    char path[64];
    DM_SNPRINTF(path, 64, PATH_FORMAT, filename);
    if (luaL_dofile(L, path) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

TEST_F(ScriptMsgTest, TestURLNewAndIndex)
{
    int top = lua_gettop(L);

    // empty
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
       ));

    // empty string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
        ));

    // default path
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\".\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
       ));

    // default fragment
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"#\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == __default_url.fragment, \"invalid fragment\")\n"
       ));

    // socket string
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test:\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    // fragment string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"#test\")\n"
        "assert(url.socket == __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == __default_url.path, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test", &socket));

    // socket arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // socket arg value
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url(\"test:\")\n"
        "local url2 = msg.url(url1.socket, \"\", \"\")\n"
        "assert(url2.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url2.path == nil, \"invalid path\")\n"
        "assert(url2.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"test\", \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", hash(\"test\"), \"\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // path arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", hash(\"test\"), nil)\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == hash(\"test\"), \"invalid path\")\n"
        "assert(url.fragment == nil, \"invalid fragment\")\n"
        ));

    // fragment arg string
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", \"test\")\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    // fragment arg value
    ASSERT_TRUE(RunString(L,
        "local url = msg.url(\"test\", \"\", hash(\"test\"))\n"
        "assert(url.socket ~= __default_url.socket, \"invalid socket\")\n"
        "assert(url.path == nil, \"invalid path\")\n"
        "assert(url.fragment == hash(\"test\"), \"invalid fragment\")\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailURLNewAndIndex)
{
    int top = lua_gettop(L);

    // invalid arg
    ASSERT_FALSE(RunString(L,
        "msg.url({})\n"
        ));
    // malformed
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test:test:\")\n"
        ));
    // socket not found
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test:\")\n"
        ));
    // invalid socket arg
    ASSERT_FALSE(RunString(L,
        "msg.url(\"\", nil, nil)\n"
        ));
    // socket arg not found
    ASSERT_FALSE(RunString(L,
        "msg.url(\"test\", nil, nil)\n"
        ));
    // path arg
    ASSERT_FALSE(RunString(L,
        "msg.url(nil, {}, nil)\n"
        ));
    // fragment arg
    ASSERT_FALSE(RunString(L,
        "msg.url(nil, nil, {})\n"
        ));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLToString)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    dmMessage::HSocket overflow_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("very_very_very_very_very_very_very_very_very_very_very_very_socket", &overflow_socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "print(tostring(url))\n"
        "url = msg.url(\"socket\", \"path\", \"test\")\n"
        "print(tostring(url))\n"
        "-- overflow\n"
        "url = msg.url(\"very_very_very_very_very_very_very_very_very_very_very_very_socket\", \"path\", \"test\")\n"
        "print(tostring(url))\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(overflow_socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLConcat)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    dmMessage::HSocket overflow_socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("very_very_very_very_very_very_very_very_very_very_very_very_socket", &overflow_socket));
    ASSERT_TRUE(RunString(L,
        "local url = msg.url()\n"
        "print(\"url: \" .. url)\n"
        "url = msg.url(\"socket\", \"path\", \"fragment\")\n"
        "print(\"url: \" .. url)\n"
        "-- overflow\n"
        "url = msg.url(\"very_very_very_very_very_very_very_very_very_very_very_very_socket\", \"path\", \"fragment\")\n"
        "print(\"url: \" .. url)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(overflow_socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLNewIndex)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url()\n"
        "local url2 = msg.url(\"socket\", nil, nil)\n"
        "url1.socket = url2.socket\n"
        "assert(url1.socket == url2.socket)\n"
        "url1.socket = nil\n"
        "assert(url1.socket ~= url2.socket)\n"
        "assert(url1.socket == nil)\n"
        "url1.socket = \"socket\"\n"
        "assert(url1.socket == url2.socket)\n"
        "url1.path = \"path\"\n"
        "url2.path = hash(\"path\")\n"
        "assert(url1.path == url2.path)\n"
        "url1.path = nil\n"
        "assert(url1.path == nil)\n"
        "url1.fragment = \"fragment\"\n"
        "url2.fragment = hash(\"fragment\")\n"
        "assert(url1.fragment == url2.fragment)\n"
        "url1.fragment = nil\n"
        "assert(url1.fragment == nil)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailURLNewIndex)
{
    int top = lua_gettop(L);

    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.socket = {}\n"
        ));
    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.path = {}\n"
        ));
    ASSERT_FALSE(RunString(L,
        "local url = msg.url()\n"
        "url.fragment = {}\n"
        ));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLEq)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));
    ASSERT_TRUE(RunString(L,
        "local url1 = msg.url(\"socket\", \"path\", \"fragment\")\n"
        "local url2 = msg.url()\n"
        "assert(url1 ~= url2)\n"
        "url2.socket = \"socket\"\n"
        "url2.path = \"path\"\n"
        "url2.fragment = \"fragment\"\n"
        "assert(url1 == url2)\n"
        "url2.socket = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.socket = \"socket\"\n"
        "assert(url1 == url2)\n"
        "url2.path = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.path = \"path\"\n"
        "assert(url1 == url2)\n"
        "url2.fragment = nil\n"
        "assert(url1 ~= url2)\n"
        "url2.fragment = \"fragment\"\n"
        "assert(url1 == url2)\n"
        "assert(url1 ~= 1)\n"
        ));
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestURLGlobal)
{
    lua_pushnil(L);
    lua_setglobal(L, DEFAULT_URL);

    ASSERT_TRUE(RunString(L,
            "local url1 = msg.url(\"default_socket:/path#fragment\")\n"
            "assert(url1.path == hash(\"/path\"))\n"
            "print(url1.fragment)\n"
            "assert(url1.fragment == hash(\"fragment\"))\n"));

    ASSERT_FALSE(RunString(L,
            "local url1 = msg.url(\"path#fragment\")\n"));
}

void DispatchCallbackDDF(dmMessage::Message *message, void* user_ptr)
{
    assert(message->m_Descriptor != 0);
    dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
    if (descriptor == TestScript::SubMsg::m_DDFDescriptor)
    {
        TestScript::SubMsg* msg = (TestScript::SubMsg*)message->m_Data;
        *((uint32_t*)user_ptr) = msg->m_UintValue;
    }
    else if (descriptor == TestScript::EmptyMsg::m_DDFDescriptor)
    {
        *((uint32_t*)user_ptr) = 2;
    }
}

struct TableUserData
{
    TableUserData() { dmMessage::ResetURL(m_URL); }

    lua_State* L;
    uint32_t m_TestValue;
    dmMessage::URL m_URL;
};

void DispatchCallbackTable(dmMessage::Message *message, void* user_ptr)
{
    assert(message->m_Id == dmHashString64("table"));
    TableUserData* user_data = (TableUserData*)user_ptr;
    dmScript::PushTable(user_data->L, (const char*)message->m_Data);
    lua_getfield(user_data->L, -1, "uint_value");
    user_data->m_TestValue = (uint32_t) lua_tonumber(user_data->L, -1);
    lua_pop(user_data->L, 2);
    assert(user_data->m_URL.m_Socket == message->m_Receiver.m_Socket);
    assert(user_data->m_URL.m_Path == message->m_Receiver.m_Path);
    assert(user_data->m_URL.m_Fragment == message->m_Receiver.m_Fragment);
}

TEST_F(ScriptMsgTest, TestPost)
{
    int top = lua_gettop(L);

    // DDF to default socket
    ASSERT_TRUE(RunString(L,
        "msg.post(\".\", \"sub_msg\", {uint_value = 1})\n"
        ));
    uint32_t test_value = 0;
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, &test_value));
    ASSERT_EQ(1u, test_value);

    // DDF to default socket
    ASSERT_TRUE(RunString(L,
        "msg.post(\"\", \"sub_msg\", {uint_value = 1})\n"
        ));
    test_value = 0;
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackDDF, &test_value));
    ASSERT_EQ(1u, test_value);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));

    // DDF
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"sub_msg\", {uint_value = 1})\n"
        ));
    test_value = 0;
    dmMessage::Dispatch(socket, DispatchCallbackDDF, &test_value);
    ASSERT_EQ(1u, test_value);

    // Empty DDF
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"empty_msg\")\n"
        ));
    test_value = 0;
    dmMessage::Dispatch(socket, DispatchCallbackDDF, &test_value);
    ASSERT_EQ(2u, test_value);

    // table
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:\", \"table\", {uint_value = 1})\n"
        ));
    TableUserData user_data;
    user_data.L = L;
    user_data.m_TestValue = 0;
    user_data.m_URL.m_Socket = socket;
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    // table, full url
    ASSERT_TRUE(RunString(L,
        "msg.post(\"socket:path2#fragment2\", \"table\", {uint_value = 1})\n"
        ));
    user_data.m_URL.m_Socket = socket;
    user_data.m_URL.m_Path = dmHashString64("path2");
    user_data.m_URL.m_Fragment = dmHashString64("fragment2");
    ASSERT_EQ(1u, dmMessage::Dispatch(socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    // table, resolve path
    ASSERT_TRUE(RunString(L,
        "msg.post(\"path2#fragment2\", \"table\", {uint_value = 1})\n"
        ));
    user_data.m_URL.m_Socket = m_DefaultURL.m_Socket;
    user_data.m_URL.m_Path = dmHashString64("path2");
    user_data.m_URL.m_Fragment = dmHashString64("fragment2");
    ASSERT_EQ(1u, dmMessage::Dispatch(m_DefaultURL.m_Socket, DispatchCallbackTable, &user_data));
    ASSERT_EQ(1u, user_data.m_TestValue);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestFailPost)
{
    int top = lua_gettop(L);

    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("socket", &socket));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"socket2:\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"#:\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_FALSE(RunString(L,
        "msg.post(\"::\", \"sub_msg\", {uint_value = 1})\n"
        ));

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::DeleteSocket(socket));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptMsgTest, TestPerf)
{
    uint64_t time = dmTime::GetTime();
    uint32_t count = 10000;
    char program[256];
    DM_SNPRINTF(program, 256,
        "local count = %u\n"
        "for i = 1,count do\n"
        "    msg.post(\"test_path\", \"table\", {uint_value = 1})\n"
        "end\n",
        count);
    ASSERT_TRUE(RunString(L, program));
    time = dmTime::GetTime() - time;
    printf("Time per post: %.4f\n", time / (double)count);
}

TEST_F(ScriptMsgTest, TestPostDeletedSocket)
{
    dmMessage::HSocket socket;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::NewSocket("test_socket", &socket));

    ASSERT_TRUE(RunString(L, "test_url = msg.url(\"test_socket:\")"));
    ASSERT_TRUE(RunString(L, "msg.post(test_url, \"test_message\")"));

    ASSERT_EQ(1u, dmMessage::Consume(socket));

    dmMessage::DeleteSocket(socket);

    ASSERT_FALSE(RunString(L, "msg.post(test_url, \"test_message\")"));
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
