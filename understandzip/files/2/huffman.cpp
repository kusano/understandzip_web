#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <functional>
using namespace std;

struct Node
{
    int symbol = 0;
    int count = 0;
    vector<Node> child;
    Node(int symbol, int count, vector<Node> child)
        :symbol(symbol), count(count), child(child) {}
    bool operator<(const Node &n) const
        {return count>n.count || count==n.count && symbol>n.symbol;}
};

vector<string> huffman(vector<int> count)
{
    priority_queue<Node> node;
    for (size_t i=0; i<count.size(); i++)
        node.push(Node((int)i, count[i], {}));
    while (node.size()>1) {
        Node n1 = node.top(); node.pop();
        Node n2 = node.top(); node.pop();
        node.push(Node(-1, n1.count+n2.count, {n1, n2}));
    }
    vector<string> code(count.size());
    function<void(const Node &, string)> traverse = [&](const Node &n, string c)
    {
        if (n.symbol != -1)
            code[n.symbol] = c;
        else {
            traverse(n.child[0], c+"0");
            traverse(n.child[1], c+"1");
        }
    };
    traverse(node.top(), "");
    return code;
}

int main()
{
    vector<string> code = huffman({1, 17, 2, 3, 5, 5});
    for (size_t i=0; i<code.size(); i++)
        cout<<i<<" "<<code[i]<<endl;
}
