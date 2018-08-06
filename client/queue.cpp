#include <iostream>
#include <queue>
using namespace std;

int main()
{
    int i;
    queue<int> test;
    queue<int> temp;
    for (i = 0; i < 10; i++)
    {
        test.push(i);
    }
    
    while(!test.empty())
    {
        temp.push(test.front());
        test.pop();
    }
   
    while(!temp.empty())
    {
        test.push(temp.front());
        temp.pop();
    }

    while(!test.empty())
    {
        cout << "<>" << test.front();
        test.pop();
    }
}