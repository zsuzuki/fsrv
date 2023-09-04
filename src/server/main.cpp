//
// Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
//
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

#if _WIN32
// wchar -> string
std::string wcs2mbs(const FilePath &src)
{
    auto *srcPtr = src.c_str();
    char buff[1024];
    wcstombs(buff, srcPtr, sizeof(buff));
    return buff;
}

// Windowsパス変換
FilePath translateToPosix(const FilePath src)
{
    auto chgStr = src.string();
    for (auto &ch : chgStr)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }
    return chgStr;
}
#endif

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
// エラーハンドラ
//
void errorHandler(const httplib::Request & /*req*/, httplib::Response &res)
{
    const char *fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html");
}

//
// ファイル情報
//
struct FileInfo
{
    FilePath path_;
    int64_t time_;
    size_t size_;
    bool delete_;
};
using FileInfoPtr = std::shared_ptr<FileInfo>;
Trie<std::string, FileInfoPtr> fileList;

//
void repliesFileList(const httplib::Request &req, httplib::Response &res)
{
    std::string prefixDir = "";
    if (req.has_param("prefix"))
    {
        prefixDir = req.get_param_value("prefix");
    }
    bool update = false;
    if (req.has_param("update"))
    {
        auto upstr = req.get_param_value("update");
        if (upstr == "1" || upstr == "true" || upstr == "TRUE" || upstr == "ON")
        {
            update = true;
        }
    }

    // ファイルリストを返す
    auto allList = fileList.searchByPrefix(prefixDir);
    nlohmann::json jsonObj;
    int findex = 0;
    for (auto &n : allList)
    {
        auto fname = n->path_.string();
        auto fsize = n->size_;
        auto ftime = n->time_;
        bool fdel  = n->delete_;
        if (update)
        {
            // 更新する場合は情報を取得
            if (std::filesystem::exists(fname))
            {
                using namespace std::chrono;
                auto lct = std::filesystem::last_write_time(fname);
                auto epl = lct.time_since_epoch();
                auto sec = duration_cast<seconds>(epl);
                ftime    = sec.count();
                fsize    = std::filesystem::file_size(fname);
                fdel     = false;
            }
            else
            {
                // 消えた…
                fsize = 0;
                ftime = 0;
                fdel  = true;
            }
            n->size_   = fsize;
            n->time_   = ftime;
            n->delete_ = fdel;
        }

        nlohmann::json entry;
        entry["Path"]     = fname;
        entry["Size"]     = fsize;
        entry["Time"]     = ftime;
        entry["Delete"]   = fdel;
        jsonObj[findex++] = entry;
    }
    nlohmann::json jsonTop;
    jsonTop["Files"] = jsonObj;

    res.set_content(jsonTop.dump(), "application/json");
}

//
// ディレクトリ情報
//
struct DirInfo
{
    using Ptr = std::shared_ptr<DirInfo>;
    FilePath path_;
    uint32_t count_;
    std::list<Ptr> children_;
};
DirInfo::Ptr currentDir;

//
nlohmann::json makeDirJson(DirInfo::Ptr dptr)
{
    nlohmann::json jdir;
    jdir["Name"]  = dptr->path_;
    jdir["Count"] = dptr->count_;
    if (!dptr->children_.empty())
    {
        nlohmann::json children;
        int idx = 0;
        for (auto c : dptr->children_)
        {
            children[idx++] = makeDirJson(c);
        }
        jdir["Children"] = children;
    }
    return jdir;
};

//
void repliesDirList(const httplib::Request &req, httplib::Response &res)
{
    if (!currentDir)
    {
        return;
    }

    nlohmann::json jsonTop;
    jsonTop["Dir"] = makeDirJson(currentDir);

    res.set_content(jsonTop.dump(), "application/json");
}

//
// ディレクトリ走査
//
bool checkDirectory(FilePath targetDir, bool dispErr = false)
{
    try
    {
        printVerbose("check dir: ", targetDir);
        auto errorDump = [&](const char *msg)
        {
            if (dispErr)
            {
                std::cerr << msg << ": " << targetDir << std::endl;
            }
        };

        if (!std::filesystem::exists(targetDir))
        {
            // 無いよ
            errorDump("not exist");
            return false;
        }
        if (!std::filesystem::is_directory(targetDir))
        {
            // ディレクトリじゃない
            errorDump("not directory");
            return false;
        }

        auto dptr    = std::make_shared<DirInfo>();
        dptr->path_  = targetDir.filename();
        dptr->count_ = 0;
        if (currentDir)
        {
            currentDir->children_.push_back(dptr);
        }
        else
        {
            printVerbose("First Directory: ", targetDir);
        }

        // ディレクトリ内全部リスト
        for (const auto &entry : std::filesystem::directory_iterator(targetDir))
        {
            if (recursiveMode && entry.is_directory())
            {
                currentDir = dptr;
                checkDirectory(entry.path());
            }
            else if (entry.is_regular_file())
            {
                using namespace std::chrono;
                auto wtime = entry.last_write_time().time_since_epoch();
                auto sec   = duration_cast<seconds>(wtime);
                auto fptr  = std::make_shared<FileInfo>();
#if _WIN32
                fptr->path_ = translateToPosix(entry.path());
#else
                fptr->path_ = entry.path();
#endif
                fptr->time_   = sec.count();
                fptr->size_   = entry.file_size();
                fptr->delete_ = false;
                fileList.insert(fptr->path_.string(), fptr);
                dptr->count_++;

                printVerbose("File: ", fptr->path_, "(", fptr->time_, ")");
            }
        }
        currentDir = dptr;
    }
    catch (std::exception &exp)
    {
        std::cerr << exp.what() << std::endl;
    }

    return true;
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
        // recursive
        "r,recursive", "recursive mode", cxxopts::value<bool>()->default_value("false"))(
        // port
        "p,port", "port number", cxxopts::value<int>())(
        // port scan
        "a,auto", "port scan", cxxopts::value<bool>()->default_value("false"))(
        // ssl enable
        "ssl", "Enable SSL", cxxopts::value<bool>()->default_value("false"))(
        // ssl certificate path
        "ssl_cert_path", "specify certificate path as argument",
        cxxopts::value<std::string>()->default_value("."))(
        // file directory
        "dir", "target directory", cxxopts::value<std::string>()->default_value("."));

    options.parse_positional({"dir"});

    auto result = options.parse(argc, argv);
    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        return 0;
    }

    verboseMode   = result["verbose"].as<bool>();
    recursiveMode = result["recursive"].as<bool>();

    // ファイルリスト収集
    FilePath targetDir{result["dir"].as<std::string>()};
    if (!checkDirectory(targetDir, true))
    {
        return 1;
    }

    // SSL使用
    std::unique_ptr<httplib::Server> svrptr;
    if (result["ssl"].as<bool>())
    {
        // SSL:ルート証明書がないとブラウザからは怒られる
        FilePath certPath = result["ssl_cert_path"].as<std::string>();
        std::cout << "enable SSL server, cert path: " << certPath << std::endl;
#if _WIN32
        auto certName = wcs2mbs(certPath / "cert.pem");
        auto keyName  = wcs2mbs(certPath / "key.pem");
#else
        FilePath certName{certPath / "cert.pem"};
        FilePath keyName{certPath / "key.pem"};
#endif
        const char *cnptr = certName.c_str();
        const char *kyptr = keyName.c_str();
        svrptr            = std::make_unique<httplib::SSLServer>(cnptr, kyptr);
    }
    else
    {
        svrptr = std::make_unique<httplib::Server>();
    }

    // サーバースタート
    httplib::Server &svr = *svrptr;
    if (!svr.is_valid())
    {
        std::cerr << "http error" << std::endl;
        return 1;
    }

    // アクセスポイント
    svr.set_error_handler(errorHandler);
    svr.Get("/list", repliesFileList);
    svr.Get("/dir", repliesDirList);

    // 絶対パスと対象ディレクトリ名
    auto abspath = std::filesystem::canonical(targetDir);
    auto dname   = abspath.filename();
    std::cout << "read dir: " << dname << "(" << abspath << ")" << std::endl;

    // マウントポイント
    std::string mountPoint = "/files/";
    std::cout << "mount point: " << dname << std::endl;

    if (!svr.set_mount_point(mountPoint, targetDir.string()))
    {
        return 1;
    }

    if (result["auto"].as<bool>())
    {
        // インターフェースを自動設定
        int port = svr.bind_to_any_port("0.0.0.0");
        std::cout << "port number: " << port << std::endl;
        svr.listen_after_bind();
    }
    else
    {
        std::cout << "start server..." << std::endl;
        auto port = DEFAULT_PORT;
        if (result.count("port"))
        {
            port = result["port"].as<int>();
        }
        printVerbose("port number: ", port);
        svr.listen("localhost", port);
    }

    return 0;
}
