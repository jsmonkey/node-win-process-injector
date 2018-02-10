#include <node.h>
#include "win_process.h"

namespace injector {

void InitAll(v8::Local<v8::Object> exports) {
    WinProcess::Init(exports);
};

NODE_MODULE(NODE_GYP_MODULE_NAME, InitAll);

} 