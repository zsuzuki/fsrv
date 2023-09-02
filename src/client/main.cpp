//
// Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
//
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
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

bool verboseMode = false; // 詳細モード

// 相対パスの"./","../"を削る
FilePath deleteDot(FilePath path)
{
    auto str = path.string();
    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] == '/')
        {
            return str.substr(i + 1);
        }
        if (str[i] != '.')
        {
            return path;
        }
    }
    return "";
}

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
void dumpDir(nlohmann::json dir, std::string tab, FilePath allPath)
{
    if (!dir.is_object())
    {
        return;
    }

    FilePath myName = dir["Name"].get<std::string>();
    auto fullPath   = allPath / myName;
    std::cout << tab << ":" << myName << "[" << fullPath << "]" << std::endl;
    if (dir["Children"].is_array())
    {
        auto children = dir["Children"];
        tab += " ";
        for (auto &ch : children.items())
        {
            auto newPath = allPath / myName;
            dumpDir(ch.value(), tab, newPath);
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
            dumpDir(dirList["Dir"], "", "");
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
                          << "(size=" << value["Size"].get<size_t>() << ")" << std::endl;
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
void syncFiles(std::string url, int port, std::string pattern)
{
    httplib::Client cli(url, port);

    httplib::Params params{{"prefix", pattern}, {"update", "true"}};
    if (auto res = cli.Get("/list", params, {}))
    {
        if (res->status == 200)
        {
            nlohmann::json fileList = nlohmann::json::parse(res->body);
            for (auto &file : fileList["Files"].items())
            {
                auto value     = file.value();
                FilePath fname = value["Path"].get<std::string>();
                auto fsize     = value["Size"].get<size_t>();
                auto ftime     = value["Time"].get<int64_t>();
                auto fdel      = value["Delete"].get<bool>();

                printVerbose(fname, ":size=", fsize, ",time=", ftime, (fdel ? "[DELETED]" : ""));
                bool needUpdate = false;
                bool fileExists = std::filesystem::exists(fname);
                if (fdel)
                {
                    if (fileExists)
                    {
                        // ファイルは消されているのでこちらも消去
                        std::cout << "remove file: " << fname << std::endl;
                        std::filesystem::remove(fname);
                    }
                }
                else if (fileExists)
                {
                    // ファイルが存在するなら更新されたか確認する
                    auto localSize = std::filesystem::file_size(fname);
                    if (localSize != fsize)
                    {
                        needUpdate = true;
                        printVerbose("  -> different file size:", localSize, "/", fsize);
                    }
                    else
                    {
                        using namespace std::chrono;
                        auto lct          = std::filesystem::last_write_time(fname);
                        auto epl          = lct.time_since_epoch();
                        auto sec          = duration_cast<seconds>(epl);
                        int64_t localTime = sec.count();

                        if (localTime < ftime)
                        {
                            needUpdate = true;
                            printVerbose("  -> different time stamp:", localTime, "/", ftime);
                        }
                    }
                }
                else
                {
                    // ない
                    needUpdate      = true;
                    auto parentPath = fname.parent_path();
                    if (!std::filesystem::exists(parentPath))
                    {
                        // ディレクトリも作る
                        std::cout << "create directorie(s): " << parentPath << std::endl;
                        std::filesystem::create_directories(parentPath);
                    }
                    printVerbose("  -> not exists(need update)");
                }
                //
                if (needUpdate)
                {
                    // ファイル更新
                    const FilePath pathPrefix{"/files"};
                    auto downloadPath = pathPrefix / deleteDot(fname);
                    std::cout << "DOWNLOAD: " << downloadPath << " -> " << fname << std::endl;
                    std::ofstream outFile{fname, std::ios::binary};
                    auto r = cli.Get(
                        downloadPath.string(), httplib::Headers(),
                        [&](const httplib::Response &response)
                        {
                            printVerbose(" response: ", response.status);
                            return true;
                        },
                        [&](const char *data, size_t data_length)
                        {
                            outFile.write(data, data_length);
                            return true;
                        },
                        [&](uint64_t current, uint64_t total)
                        {
                            std::cout << current << "/" << total << '\r';
                            return true;
                        });
                    if (r->status == 200)
                    {
                        std::cout << "Download size: " << fsize << " ===> done." << std::endl;
                    }
                }
            }
        }
    }
    else
    {
        auto err = res.error();
        std::cout << "HTTP error: " << httplib::to_string(err) << std::endl;
    }
}

} // namespace

//
//
//
int main(int argc, char **argv)
{
    cxxopts::Options options(argv[0], "file synchronize server");

    options.add_options()("h,help", "Print usage")(
        // verbose
        "v,verbose", "verbose mode", cxxopts::value<bool>()->default_value("false"))(
        // port
        "p,port", "port number", cxxopts::value<int>())(
        // file directory
        "url", "target url", cxxopts::value<std::string>()->default_value("localhost"))(
        // command
        "command", "command [dir,files,sync]", cxxopts::value<std::string>()->default_value("dir"))(
        // match pattern
        "pattern", "matching pattern", cxxopts::value<std::string>()->default_value(""));

    options.parse_positional({"url", "command", "pattern"});

    auto result = options.parse(argc, argv);
    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    verboseMode = result["verbose"].as<bool>();

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
