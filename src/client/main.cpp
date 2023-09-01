//
// Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
//
#include "nlohmann/json_fwd.hpp"
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <httplib.h>
#include <iostream>
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <trie.h>

namespace
{
using FilePath = std::filesystem::path;

bool verboseMode   = false; // 詳細モード
bool recursiveMode = false; // 再帰検索モード

// デバッグ表示
template <class Msg> void printVerbose(Msg msg)
{
    if (verboseMode)
    {
        std::cout << msg << std::endl;
    }
}
template <class Msg, class... Args> void printVerbose(Msg msg, Args... args)
{
    if (verboseMode)
    {
        std::cout << msg;
        printVerbose(args...);
    }
}

//
// ディレクトリリスト
//
void dumpDir(nlohmann::json dir, std::string tab)
{
    if (!dir.is_object())
    {
        return;
    }

    std::cout << tab << "Name: " << dir["Name"].get<std::string>() << std::endl;
    if (dir["Children"].is_array())
    {
        auto children = dir["Children"];
        tab += " ";
        for (auto &ch : children.items())
        {
            dumpDir(ch.value(), tab);
        }
    }
}

//
void getDirectory(std::string url, int port, std::string pattern)
{
    httplib::Client cli(url, port);

    if (auto res = cli.Get("/dir"))
    {
        if (res->status == 200)
        {
            nlohmann::json dirList = nlohmann::json::parse(res->body);
            dumpDir(dirList["Dir"], "");
        }
    }
    else
    {
        auto err = res.error();
        std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
    }
}

//
// ファイルリスト
//
void getFileList(std::string url, int port, std::string pattern)
{
    httplib::Client cli(url, port);

    httplib::Params params{{"prefix", pattern}};
    if (auto res = cli.Get("/list", params, {}))
    {
        if (res->status == 200)
        {
            nlohmann::json fileList = nlohmann::json::parse(res->body);
            for (auto &file : fileList["Files"].items())
            {
                auto value = file.value();
                std::cout << value["Path"].get<std::string>()
                          << "(size=" << value["Size"].get<size_t>() << ")"
                          << std::endl;
            }
        }
    }
    else
    {
        auto err = res.error();
        std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
    }
}

//
// ファイル同期
//
void syncFiles(std::string url, int port, std::string pattern) {}

} // namespace

//
//
//
int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "file synchronize server");

    options.add_options()("h,help", "Print usage")(
        // verbose
        "v,verbose", "verbose mode",
        cxxopts::value<bool>()->default_value("false"))(
        // recursive
        "r,recursive", "recursive mode",
        cxxopts::value<bool>()->default_value("false"))(
        // port
        "p,port", "port number", cxxopts::value<int>())(
        // file directory
        "url", "target url",
        cxxopts::value<std::string>()->default_value("localhost"))(
        // command
        "command", "command [dir,files,sync]",
        cxxopts::value<std::string>()->default_value("dir"))(
        // match pattern
        "pattern", "matching pattern",
        cxxopts::value<std::string>()->default_value(""));

    options.parse_positional({"url", "command", "pattern"});

    auto result = options.parse(argc, argv);
    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    verboseMode   = result["verbose"].as<bool>();
    recursiveMode = result["recursive"].as<bool>();

    auto url  = result["url"].as<std::string>();
    auto port = DEFAULT_PORT;
    if (result.count("port"))
    {
        port = result["port"].as<int>();
    }
    printVerbose("port number: ", port);

    auto command = result["command"].as<std::string>();
    auto pattern = result["pattern"].as<std::string>();

    if (command == "dir")
    {
        getDirectory(url, port, pattern);
    }
    else if (command == "files")
    {
        getFileList(url, port, pattern);
    }
    else if (command == "sync")
    {
        syncFiles(url, port, pattern);
    }
    else
    {
        std::cerr << "unsupport command: " << command << std::endl;
    }

    return 0;
}
