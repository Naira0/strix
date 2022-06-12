#pragma once

#include <stdexcept>

template<class T>
class Stack
{
public:

    struct Node
    {
        T     data;
        Node *last;
        Node *next;
    };

    Stack() :
        m_head(nullptr),
        m_tail(nullptr),
        m_size(0)
    {}

    ~Stack()
    {
        Node *node = m_tail;

        while(node)
        {
            Node *last = node->last;
            delete node;
            node = last;
        }
    }

    template<typename ...V>
    inline void emplace(V &&...item)
    {
        auto node = new Node{T(std::forward<V>(item)...), m_tail, nullptr};
        _push(node);
    }

    inline void push(T &&item)
    {
        auto node = new Node{std::forward<T>(item), m_tail, nullptr};
        _push(node);
    }

    inline void push(const T &item)
    {
        auto node = new Node(item, m_tail, nullptr);
        _push(node);
    }

    T pop()
    {
        if(m_tail == nullptr)
            throw std::exception("tail is null");

        Node *temp = m_tail;

        T value = std::move(temp->data);

        m_tail = m_tail->last;

        delete temp;

        if(m_tail)
            m_tail->next = nullptr;

        m_size--;

        return value;
    }

    inline T& top() const
    {
        if(m_tail == nullptr)
            throw std::out_of_range("back of stack is empty");
        return m_tail->data;
    }

    inline T& bottom() const
    {
        if(m_head == nullptr)
            throw std::out_of_range("front of stack is empty");
        return m_head->data;
    }

    constexpr inline bool
    empty() const
    {
        return m_size == 0;
    }

    constexpr size_t
    size() const
    {
        return m_size;
    }

    inline std::pair<T&, T&>
    top_two() const
    {
        return {m_tail->last->data, m_tail->data};
    }

    inline Node* tail_node() const
    {
        return const_cast<Node*>(m_tail);
    }

private:
    Node *m_head;
    Node *m_tail;
    size_t m_size;

    void _push(Node *node)
    {
        if(!m_head)
            m_head = node;
        if(m_tail)
            m_tail->next = node;

        m_tail = node;
        m_size++;
    }
};