#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include <nlohmann/json.hpp>

namespace params {
using SerializableType =
    std::variant<std::string, bool, int, unsigned int, float, double, long,
                 unsigned long, short, unsigned short>;

template <class T, class U> struct includes;
template <class T, class... Ts>
struct includes<T, std::variant<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};
template <class T> using serializable = includes<T, SerializableType>;

template <typename T> class P0 {
public:
  inline const std::string &name() const { return name_; }

  inline operator T &() { return value_; }
  inline operator const T &() const { return value_; }

  inline const T &value() const { return value_; }
  inline T &value() { return value_; }

  friend std::ostream &operator<<(std::ostream &stream, const P0<T> &p) {
    if (p.name_.empty())
      return stream << p.value_;
    return stream << p.name_ << '=' << p.value_;
  }

protected:
  T value_{};
  std::string name_{""};

  P0() = default;
  P0(const T &value, const std::string &name = "")
      : value_(value), name_(name) {}
};

using SerializableProperty =
    std::variant<P0<std::string> *, P0<bool> *, P0<int> *, P0<unsigned int> *,
                 P0<float> *, P0<double> *, P0<long> *, P0<unsigned long> *,
                 P0<short> *, P0<unsigned short> *>;

inline std::ostream &operator<<(std::ostream &stream,
                                const SerializableProperty &pvar) {
  std::visit([&stream](auto &property) { stream << *property; }, pvar);
  return stream;
}

struct Group {
public:
  Group(const std::string &name = "", Group *parent = nullptr) : name_(name) {
    if (parent != nullptr)
      parent->subgroups_[name] = this;
  }
  Group(const char *name, Group *parent = nullptr) : name_(name) {
    if (parent != nullptr)
      parent->subgroups_[name] = this;
  }

  std::unordered_map<std::string, SerializableProperty> &members() {
    return members_;
  }
  const std::unordered_map<std::string, SerializableProperty> &members() const {
    return members_;
  }

  std::unordered_map<std::string, Group *> &subgroups() { return subgroups_; }
  const std::unordered_map<std::string, Group *> &subgroups() const {
    return subgroups_;
  }

  friend std::ostream &operator<<(std::ostream &stream, const Group &g) {
    stream << '<' << g.name_ << '>' << std::endl;
    for (const auto &member : g.members_)
      stream << '\t' << member.second << std::endl;
    for (const auto &member : g.subgroups_)
      stream << *member.second << std::endl;
    stream << "</" << g.name_ << '>';
    return stream;
  }

  template <typename T> T &get(const std::string &name) {
    return std::get<P0<T> *>(members_[name])->value();
  }
  template <typename T> const T &get(const std::string &name) const {
    return std::get<P0<T> *>(members_.at(name))->value();
  }

  const std::string &name() const { return name_; }
  std::string &name() { return name_; }

private:
  std::string name_;
  std::unordered_map<std::string, Group *> subgroups_;

  std::unordered_map<std::string, SerializableProperty> members_;
};

template <typename T> class Property : public P0<T> {
public:
  using P0<T>::P0;

  explicit Property(Group *group = nullptr) : P0<T>(), group_(group) {
    if constexpr (serializable<T>{}) {
      if (group != nullptr)
        group->members()[this->template name_] = this;
    }
  }

  Property(const T &value, const std::string &name = "", Group *group = nullptr)
      : P0<T>(value, name), group_(group) {
    if constexpr (serializable<T>{}) {
      if (group != nullptr)
        group->members()[this->template name_] = this;
    }
  }

protected:
  Group *group_{nullptr};
};

using nlohmann::json;
template <typename T>
inline std::enable_if_t<serializable<T>{}> to_json(json &j, const P0<T> &p) {
  j[p.name()] = p.value();
}

inline void to_json(json &j, const Group &g) {
  auto &j_ = j[g.name()];
  for (const auto &subgroup : g.subgroups())
    to_json(j_[subgroup.first], *subgroup.second);
  for (const auto &member : g.members()) {
    std::visit([&](const auto *p) { to_json(j_[member.first], *p); },
               member.second);
  }
}

} // namespace params
