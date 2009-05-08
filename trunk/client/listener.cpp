#include "client/listener.hpp"
void Listener::Receive(const ProtobufLineFormat *line_format) {
  if (line_format == NULL) {
    LOG(WARNING) << "Reply line format is null";
    return;
  }
  VLOG(3) << "Receive  "
          << " content: " << line_format->body
          << " size: " << line_format->body.size();
  HandlerTable::const_iterator it = handler_table_.find(line_format->name);
  if (it == handler_table_.end()) {
    LOG(WARNING) << "receive unlisten message: "
                 << line_format->name;
    return;
  }
  it->second(line_format);
  return;
}
