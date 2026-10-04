
#include <algorithm>
#include <cassert>
#include <compare>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>
template <typename T> class not_null {
 T ptr;
public:
 not_null(T p):ptr(p) { assert(p); }
 template<typename U> not_null(const not_null<U> &other):ptr(other.get()) {}
 T get() const { return ptr; }
 T operator->() const { return ptr; }
 operator T() const { return ptr; }
};
struct PeerId { int value = 1; };
struct PeerData { PeerId id; };
struct FullMsgId { int peer = 0; int msg = 0; auto operator<=>(const FullMsgId&) const = default; };
struct HistoryMessageForwarded {};
struct String : std::string { using std::string::string; using std::string::operator=; bool isEmpty() const { return empty(); } };
struct Text { String text; bool operator==(const Text&) const = default; };
struct HistoryItem;
struct Owner;
struct History {
 PeerData *peer;
 Owner *data;
 std::vector<std::unique_ptr<HistoryItem>> entries;
 const auto &items() const { return entries; }
 Owner &owner() const { return *data; }
};
struct HistoryItem {
 int id;
 History *h;
 bool regular = true;
 bool service = false;
 PeerData *sender;
 PeerData *original;
 bool forwarded = false;
 std::string signature;
 Text text{ "same message" };
 int reply = 0;
 History *history() const { return h; }
 bool isRegular() const { return regular; }
 bool isService() const { return service; }
 const Text &originalText() const { return text; }
 PeerData *from() const { return sender; }
 PeerData *originalSender() const { return original; }
 const std::string &originalPostAuthor() const { return signature; }
 int replyTo() const { return reply; }
 FullMsgId fullId() const { return {h->peer->id.value,id}; }
 template<typename T> const T *Get() const { static T value; return forwarded ? &value : nullptr; }
};
struct Owner {
 History *h;
 HistoryItem *message(PeerId, int id) const {
  for (const auto &item : h->entries) if(item->id == id) return item.get();
  return nullptr;
 }
 HistoryItem *message(FullMsgId id) const { return message({},id.msg); }
 void requestItemViewRefresh(HistoryItem*) {}
};
namespace base { template<typename T> struct flat_set : std::set<T> { void remove(const T &v) { this->erase(v); } }; }
namespace crl { template<typename F> void on_main(F) {} }
struct AyuSettings {
 static AyuSettings &getInstance() { static AyuSettings s; return s; }
 bool collapseDuplicates() const { return true; }
};
namespace FiltersController {
std::set<int> showingFilteredMessages;
bool isEnabled(PeerData*) { return true; }
#include "duplicate_group_under_test.h"

}
int main() {
 using namespace FiltersController;
 PeerData chat, self, alice, bob;
 Owner owner{};
 History history{&chat,&owner,{}};
 owner.h=&history;
 auto add = [&](int id, bool real, PeerData *author, bool forward=true) {
  auto item=std::make_unique<HistoryItem>();
  item->id=id;item->h=&history;item->regular=real;
  item->sender=&self;item->original=author;item->forwarded=forward;
  auto result=item.get();history.entries.push_back(std::move(item));return result;
 };
 auto first=add(1,true,&alice);
 auto second=add(2,true,&bob);
 assert(countDuplicateGroupSize(first)==1);
 assert(countDuplicateGroupSize(second)==1);
 assert(!getDuplicateHead(second));
 second->original=&alice;
 assert(countDuplicateGroupSize(first)==2);
 assert(getDuplicateHead(second)==first);
 auto previewA=add(1000,false,&alice);
 auto previewB=add(1001,false,&alice);
 assert(countDuplicateGroupSize(first)==2);
 assert(countDuplicateGroupSize(previewA)==1);
 assert(countDuplicateGroupSize(previewB)==1);
 assert(!isDuplicateMessage(previewA));
 auto realThird=add(2000,true,&alice);
 assert(countDuplicateGroupSize(first)==3);
 assert(getDuplicateHead(realThird)==first);
 assert(countDuplicateGroupSize(previewA)==1);
 first->original=nullptr;second->original=nullptr;realThird->original=nullptr;
 assert(countDuplicateGroupSize(first)==1);
 assert(!getDuplicateHead(second));
 first->forwarded=false;second->forwarded=false;realThird->forwarded=false;
 assert(countDuplicateGroupSize(first)==3);
 second->forwarded=true;second->original=&alice;
 assert(countDuplicateGroupSize(first)==1);
 first->forwarded=true;first->original=&alice;
 first->signature="author A";second->signature="author B";
 assert(countDuplicateGroupSize(first)==1);
 first->signature=second->signature;
 assert(countDuplicateGroupSize(first)==2);
 second->reply=7;
 assert(countDuplicateGroupSize(first)==1);
 second->reply=0;second->text.text="different";
 assert(countDuplicateGroupSize(first)==1);
 std::cout << "Duplicate grouping checks passed: authors, previews, counts, signatures, hidden authors, replies and text.\n";
}
