//
// Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
// base written by Chat-GPT4
//
#pragma once

#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

//
template <class Body> class TrieNode
{
  public:
    using Ptr = std::unique_ptr<TrieNode>;
    std::unordered_map<char, Ptr> children;
    Body body{};
    bool isEndOfWord{false}; // ノードが単語の終端を示す場合はtrue
};

//
template <class Key, class Body> class Trie
{
  public:
    using MyNode = TrieNode<Body>;

  private:
    std::unique_ptr<MyNode> root{std::make_unique<MyNode>()};

  public:
    Trie()          = default;
    virtual ~Trie() = default;

    //
    void insert(const Key &word, Body bodyarg)
    {
        MyNode *node = root.get();
        for (char c : word)
        {
            if (node->children.find(c) == node->children.end())
            {
                node->children[c]       = std::make_unique<MyNode>();
                node->children[c]->body = bodyarg;
            }
            node = node->children[c].get();
        }
        node->isEndOfWord = true;
    }

    //
    std::optional<Body> search(const Key &word)
    {
        MyNode *node = root.get();
        for (char c : word)
        {
            if (node->children.find(c) == node->children.end())
            {
                return std::nullopt;
            }
            node = node->children[c].get();
        }
        if (node->isEndOfWord)
        {
            return node->body;
        }
        return std::nullopt;
    }

    //
    std::list<Body> searchByPrefix(const Key &prefix)
    {
        std::list<Body> results;
        MyNode *node = root.get();

        // プレフィックスまでのノードを探索
        for (char c : prefix)
        {
            if (node->children.find(c) == node->children.end())
            {
                return results; // プレフィックスが存在しない場合、空のリストを返す
            }
            node = node->children[c].get();
        }

        // この時点で、nodeはプレフィックスの最後の文字を指しています
        findAllWordsWithPrefix(node, results);
        return results;
    }

    //
    bool remove(const Key &word) { return _remove(root, word, 0); }

  private:
    void findAllWordsWithPrefix(MyNode *node, std::list<Body> &results)
    {
        if (node->isEndOfWord)
        {
            results.push_back(node->body);
        }
        for (const auto &pair : node->children)
        {
            findAllWordsWithPrefix(pair.second.get(), results);
        }
    }

    bool _remove(typename MyNode::Ptr &node, const std::string &word, int index)
    {
        if (index == word.size())
        {
            if (node->isEndOfWord)
            {
                node->isEndOfWord = false;
                return node->children.empty();
            }
            return false;
        }

        char c = word[index];
        if (node->children.find(c) == node->children.end())
        {
            return false;
        }

        bool shouldDeleteChild = _remove(node->children[c], word, index + 1);

        if (shouldDeleteChild)
        {
            node->children[c].reset();
            node->children.erase(c);
            return node->children.empty() && !node->isEndOfWord;
        }

        return false;
    }
};
