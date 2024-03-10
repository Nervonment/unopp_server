#ifndef RESPONSOR_HPP
#define RESPONSOR_HPP

#include <functional>
#include <string>
#include <vector>

template <typename SendMsgFunc>
class Responsor {
protected:
    SendMsgFunc send_msg;

public:
    Responsor(const SendMsgFunc& send_msg) :send_msg(send_msg) {}
};

#endif
