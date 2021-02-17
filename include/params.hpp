#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

#include "json.hpp"

namespace params {
class P0 {
 public:
  inline const std::string &name() const { return name_; }

  virtual void serialize(nlohmann::json &j) const = 0;
  virtual void serialize(std::ostream &stream) const = 0;

  friend std::ostream &operator<<(std::ostream &stream, const P0 &p) {
    return stream << p.name_;
  }

  virtual void load(const nlohmann::json &j, bool fail_if_not_found) = 0;

 protected:
  std::string name_{""};
  P0(const std::string &name) : name_(name) {}
};

struct Group {
  Group(const std::string &name = "", Group *parent = nullptr) : name_(name) {
    if (parent != nullptr) parent->subgroups_[name] = this;
  }
  Group(const char *name, Group *parent = nullptr) : name_(name) {
    if (parent != nullptr) parent->subgroups_[name] = this;
  }

  std::unordered_map<std::string, P0 *> &members() { return members_; }
  const std::unordered_map<std::string, P0 *> &members() const {
    return members_;
  }

  std::unordered_map<std::string, Group *> &subgroups() { return subgroups_; }
  const std::unordered_map<std::string, Group *> &subgroups() const {
    return subgroups_;
  }

  friend std::ostream &operator<<(std::ostream &stream, const Group &g) {
    if (!g.name_.empty()) {
      stream << '<' << g.name_ << '>' << std::endl;
    }
    for (const auto &member : g.members_) {
      stream << '\t';
      if (member.second) {
        member.second->serialize(stream);
      }
      stream << std::endl;
    }
    for (const auto &member : g.subgroups_) {
      if (member.second) {
        stream << *member.second << std::endl;
      }
    }
    if (!g.name_.empty()) {
      stream << "</" << g.name_ << '>';
    }
    return stream;
  }

  const std::string &name() const { return name_; }

  void load(const nlohmann::json &j, bool fail_if_not_found = false) {
    if (!name_.empty() && j.find(name_) == j.end()) {
      if (fail_if_not_found) {
        assert(false);
        throw std::runtime_error(
            "Could not find definitions for parameter group \"" + name_ +
            "\".");
      }
      return;
    }
    const auto &subj = name_.empty() ? j : j[name_];
    for (const auto &member : members_) {
      member.second->load(subj, fail_if_not_found);
    }
    for (const auto &member : subgroups_) {
      member.second->load(subj, fail_if_not_found);
    }
  }

  void serialize(nlohmann::json &j) const {
    if (name_.empty()) {
      for (const auto &member : members_) member.second->serialize(j);
      for (const auto &member : subgroups_) member.second->serialize(j);
    } else {
      nlohmann::json &child = j[name_];
      for (const auto &member : members_) member.second->serialize(child);
      for (const auto &member : subgroups_) member.second->serialize(child);
    }
  }

  void add_subgroup(Group *group) {
    assert(group);
    subgroups_[group->name_] = group;
  }

 private:
  std::string name_;
  std::unordered_map<std::string, Group *> subgroups_;
  std::unordered_map<std::string, P0 *> members_;
};

template <typename T>
class Property : public P0 {
 public:
  explicit Property(Group *group = nullptr) : P0(""), group_(group) {
    if (group != nullptr) group->members()[name_] = this;
  }

  explicit Property(const T &value, const std::string &name = "",
                    Group *group = nullptr)
      : P0(name), value_(value), group_(group) {
    if (group != nullptr) group->members()[name_] = this;
  }

  inline operator T &() { return value_; }
  inline operator const T &() const { return value_; }

  inline const T &value() const { return value_; }
  inline T &value() { return value_; }

  Property<T> &operator=(const T &v) {
    value_ = v;
    return *this;
  }

  bool operator==(const Property<T> &other) const {
    return name_ == other.name_ && value_ == other.value_;
  }

  friend std::ostream &operator<<(std::ostream &stream, const Property<T> &p) {
    if (p.name_.empty()) return stream << p.value_;
    return stream << p.name_ << " = " << p.value_;
  }

  void serialize(nlohmann::json &j) const override { j[name_] = value_; }
  virtual void serialize(std::ostream &stream) const override {
    stream << name_ << " = " << value_;
  }

  void load(const nlohmann::json &j, bool fail_if_not_found) override {
    if (j.find(name_) == j.end()) {
      if (fail_if_not_found) {
        if (group_ && !group_->name().empty()) {
          throw std::runtime_error("Could not find setting for parameter \"" +
                                   name_ + "\" from group \"" + group_->name() +
                                   "\".");
        } else {
          throw std::runtime_error("Could not find setting for parameter \"" +
                                   name_ + "\".");
        }
      }
      return;
    }
    deserialize_(value_, j[name_], fail_if_not_found);
  }

 protected:
  T value_{};
  Group *group_{nullptr};

  // special deserialization for properties containing a list of groups
  template <typename GroupType>
  static std::enable_if_t<std::is_base_of<Group, GroupType>::value>
  deserialize_(std::vector<GroupType *> &value, const nlohmann::json &j,
               bool fail_if_not_found) {
    value.clear();
    for (const auto &object : j) {
      GroupType *group = new GroupType;
      for (const auto &member : group->members()) {
        member.second->load(object, fail_if_not_found);
      }
      for (const auto &member : group->subgroups()) {
        member.second->load(object, fail_if_not_found);
      }
      value.push_back(group);
    }
  }

  template <typename U>
  static void deserialize_(U &value, const nlohmann::json &j, bool) {
    value = j.get<U>();
  }
};

template <typename U>
inline std::ostream &operator<<(std::ostream &stream, const std::vector<U> &p) {
  stream << '[';
  for (auto i = 0u; i < p.size(); ++i) {
    stream << p[i];
    if (i < p.size() - 1) stream << ", ";
  }
  return stream << ']';
}

using nlohmann::json;

// special serialization for properties containing a list of groups
template <typename GroupType>
static inline std::enable_if_t<std::is_base_of<Group, GroupType>::value>
to_json(json &j, const std::vector<GroupType *> &value) {
  j = {};
  for (const auto &group : value) {
    json g;
    // to_json(j, group);
    group->serialize(g);
    // group.serialize(g);
    j.push_back(g);
  }
}

template <typename T>
static inline void to_json(json &j, const Property<T> &p) {
  j[p.name()] = p.value();
}

template <typename GroupType>
static inline std::enable_if_t<std::is_base_of<Group, GroupType>::value ||
                               std::is_same<GroupType, Group>::value>
to_json(json &j, const GroupType &g) {
  auto &j_ = j[g.name()];
  for (const auto &subgroup : g.subgroups()) {
    to_json(j_, *subgroup.second);
  }
  for (const auto &member : g.members()) {
    member.second->serialize(j_);
  }
}
}  // namespace params
