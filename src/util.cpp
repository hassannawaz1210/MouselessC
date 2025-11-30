#include <vector>
#include <cmath>

std::vector<int> find_divisors(const int num)
{
    std::vector<int> divisors;
    int sqrt_num = std::sqrt(num);
    for (int i=1; i<=sqrt_num; i++) {
       if (num%i == 0) {
            divisors.push_back(i); 
       int division = num/i;
        if (i != division)
        {
            divisors.push_back(division);
        } 
    }
   }  
    return divisors;
}

//TODO: make it more efficient
std::vector<int> common_divisors(const std::vector<int> &one, const std::vector<int> &two)
{
    std::vector<int> result;
    for (size_t i=0; i<one.size(); i++)
    {
       for (size_t j=0; j<two.size(); j++) {
            if (one[i] ==  two[j]) {
                result.push_back(one[i]);
                break;
            }
       } 
    }
    return result;
}
