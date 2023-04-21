//
// Created by zhenxi on 2022/1/21.
//

#ifndef QCONTAINER_PRO_STRINGHANDLER_H
#define QCONTAINER_PRO_STRINGHANDLER_H

#include <list>
#include <AllInclude.h>

class stringHandler {
public:

    static void hookStrHandler(bool hookAll,
                               const std::list<string> &forbid_list,
                               const std::list<string> &filter_list,
                               std::ofstream *os);

    static void stopjnitrace();

private:
    static void init();
};


#endif //QCONTAINER_PRO_STRINGHANDLER_H
