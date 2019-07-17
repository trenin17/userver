#pragma once

#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <boost/numeric/conversion/cast.hpp>
#include <boost/optional.hpp>

#include <engine/mutex.hpp>
#include <formats/json/serialize_container.hpp>
#include <formats/json/value.hpp>
#include <utils/meta.hpp>
#include <yaml_config/value.hpp>

namespace taxi_config {

class DocsMap final {
 public:
  /* Returns config item or throws an exception if key is missing */
  formats::json::Value Get(const std::string& name) const;

  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  void Set(std::string name, formats::json::Value);
  void Parse(const std::string& json, bool empty_ok);
  size_t Size() const;

  auto GetMap() const { return docs_; }
  void MergeFromOther(DocsMap&& other);
  std::vector<std::string> GetRequestedNames() const;

  std::string AsJsonString() const;

 private:
  std::unordered_map<std::string, formats::json::Value> docs_;
  mutable std::unordered_set<std::string> requested_names_;
};

namespace impl {

template <typename Res>
Res Parse(const std::string& name, const DocsMap& mongo_docs) {
  auto const element = mongo_docs.Get(name);
  return element.As<Res>();
}

}  // namespace impl

template <typename T>
class Value final {
 public:
  Value(const std::string& name, const DocsMap& mongo_docs)
      : value_(impl::Parse<T>(name, mongo_docs)) {}

  operator const T&() const { return value_; }

  const T& Get() const { return value_; }
  const T* operator->() const { return &value_; }

 private:
  T value_;
};

extern const std::string kValueDictDefaultName;

template <typename ValueType>
class ValueDict final {
 public:
  using DictType = std::unordered_map<std::string, ValueType>;

  ValueDict() = default;
  ValueDict(const std::string& name, const DocsMap& mongo_docs);
  ValueDict(std::string name, DictType dict)
      : name_(std::move(name)), dict_(std::move(dict)) {}

  bool HasDefaultValue() const { return HasValue(kValueDictDefaultName); }

  bool HasValue(const std::string& key) const {
    return dict_.find(key) != dict_.end();
  }

  const ValueType& GetDefaultValue() const {
    const auto it = dict_.find(kValueDictDefaultName);
    if (it == dict_.end()) {
      throw std::runtime_error("no value for '" + kValueDictDefaultName + '\'' +
                               (name_.empty() ? "" : " in " + name_));
    }
    return it->second;
  }

  const ValueType& operator[](const std::string& key) const {
    auto it = dict_.find(key);
    if (it == dict_.end()) {
      it = dict_.find(kValueDictDefaultName);
      if (it == dict_.end())
        throw std::runtime_error("no value for '" + key + '\'' +
                                 (name_.empty() ? "" : " in " + name_));
    }
    return it->second;
  }

  const ValueType& operator[](const boost::optional<std::string>& key) const {
    if (key) return (*this)[*key];
    return GetDefaultValue();
  }

  const ValueType& Get(const std::string& key) const { return (*this)[key]; }

  boost::optional<ValueType> GetOptional(const std::string& key) const {
    auto it = dict_.find(key);
    if (it == dict_.end()) {
      it = dict_.find(kValueDictDefaultName);
      if (it == dict_.end()) return boost::none;
    }

    return it->second;
  }

 private:
  std::string name_;
  DictType dict_;
};

template <typename T>
ValueDict<T> Parse(const formats::json::Value& elem,
                   formats::parse::To<ValueDict<T>>) {
  return {elem.GetPath(), elem.As<typename ValueDict<T>::DictType>()};
}

template <typename ValueType>
ValueDict<ValueType>::ValueDict(const std::string& name,
                                const DocsMap& mongo_docs)
    : ValueDict<ValueType>(mongo_docs.Get(name).As<ValueDict<ValueType>>()) {}

}  // namespace taxi_config
