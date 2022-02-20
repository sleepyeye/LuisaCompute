#pragma vengine_package vengine_database

#include <vstl/config.h>
#include <serde/SimpleBinaryJson.h>
#include <serde/SimpleJsonValue.h>
#include <vstl/Common.h>
#include <vstl/StringUtility.h>
namespace toolhub::db {
//Bind Python Object
class DictIEnumerator : public vstd::IEnumerable<JsonKeyPair> {
public:
    KVMap::Iterator ite;
    KVMap::Iterator end;
    DictIEnumerator(
        KVMap::Iterator &&ite,
        KVMap::Iterator &&end)
        : ite(ite),
          end(end) {}
    JsonKeyPair GetValue() override {
        return JsonKeyPair{ite->first.GetKey(), ite->second.GetVariant()};
    };
    bool End() override {
        return ite == end;
    };
    void GetNext() override {
        ++ite;
    };
};
class ArrayIEnumerator : public vstd::IEnumerable<ReadJsonVariant> {
public:
    SimpleJsonVariant const *ite;
    SimpleJsonVariant const *end;
    ArrayIEnumerator(
        SimpleJsonVariant const *ite,
        SimpleJsonVariant const *end)
        : ite(ite),
          end(end) {}
    ReadJsonVariant GetValue() override {
        return ite->GetVariant();
    };
    bool End() override {
        return ite == end;
    };
    void GetNext() override {
        ++ite;
    };
};
template<typename Func>
class MoveDictIEnumerator : public vstd::IEnumerable<MoveJsonKeyPair> {
public:
    Func f;
    KVMap::Iterator ite;
    KVMap::Iterator end;
    MoveDictIEnumerator(
        Func &&func,
        KVMap::Iterator &&ite,
        KVMap::Iterator &&end)
        : ite(ite),
          f(std::move(func)),
          end(end) {}
    ~MoveDictIEnumerator() { f(); }
    MoveJsonKeyPair GetValue() override {
        return MoveJsonKeyPair{ite->first.GetKey(), std::move(ite->second.value)};
    };
    bool End() override {
        return ite == end;
    };
    void GetNext() override {
        ++ite;
    };
};
template<typename Func>
class MoveArrayIEnumerator : public vstd::IEnumerable<WriteJsonVariant> {
public:
    Func f;
    SimpleJsonVariant *ite;
    SimpleJsonVariant *end;
    MoveArrayIEnumerator(
        Func &&func,
        SimpleJsonVariant *ite,
        SimpleJsonVariant *end)
        : ite(ite),
          f(std::move(func)),
          end(end) {}
    ~MoveArrayIEnumerator() { f(); }
    WriteJsonVariant GetValue() override {
        return std::move(ite->value);
    };
    bool End() override {
        return ite == end;
    };
    void GetNext() override {
        ++ite;
    };
};
struct BinaryHeader {
    uint64 preDefine;
    uint64 size;
    uint64 postDefine;
};
static void SerPreProcess(vstd::vector<uint8_t> &data) {
    data.resize(data.size() + sizeof(uint64));
}
static bool IsMachineLittleEnding() {
    uint i = 0x12345678;
    uint8_t *p = reinterpret_cast<uint8_t *>(&i);
    return (*p == 0x78) && (*(p + 1) == 0x56);
}
template<bool isDict>
static void SerPostProcess(vstd::vector<uint8_t> &data, size_t initOffset) {
    uint64 hashValue;
    vstd::hash<BinaryHeader> hasher;
    if constexpr (isDict) {
        hashValue = hasher(BinaryHeader{3551484578062615571ull, (data.size() - initOffset), 13190554206192427769ull});
    } else {
        hashValue = hasher(BinaryHeader{917074095154020627ull, (data.size() - initOffset), 12719994496415311585ull});
    }
    *reinterpret_cast<uint64 *>(data.data() + initOffset) = hashValue;
}
template<bool isDict>
static bool DeserCheck(vstd::span<uint8_t const> &sp, bool &sameEnding) {
    uint64 hashValue;
    vstd::hash<BinaryHeader> hasher;
    auto ending = vstd::SerDe<uint8_t>::Get(sp);
    sameEnding = (bool(ending) == IsMachineLittleEnding());
    if constexpr (isDict) {
        hashValue = hasher(BinaryHeader{3551484578062615571ull, sp.size(), 13190554206192427769ull});
    } else {
        hashValue = hasher(BinaryHeader{917074095154020627ull, sp.size(), 12719994496415311585ull});
    }
    auto hashInside = vstd::SerDe<uint64>::Get(sp);
    if (!sameEnding)
        vstd::ReverseBytes(hashInside);
    return hashValue == hashInside;
}
static void PrintString(vstd::string const &str, vstd::string &result) {
    result << '\"';
    char const *last = str.data();
    auto Flush = [&](char const *ptr) {
        result.append(last, ptr - last);
        last = ptr + 1;
    };
    for (auto &&i : str) {
        switch (i) {
            case '\t':
                Flush(&i);
                result += "\\t"sv;
                break;
            case '\r':
                Flush(&i);
                result += "\\r"sv;
                break;
            case '\n':
                Flush(&i);
                result += "\\n"sv;
                break;
            case '\'':
                Flush(&i);
                result += "\\\'"sv;
                break;
            case '\"':
                Flush(&i);
                result += "\\\""sv;
                break;
        }
    }
    Flush(str.data() + str.size());
    result << '\"';
}
template<typename Dict, typename Array>
static void PrintSimpleJsonVariant(SimpleJsonVariant const &v, vstd::string &str, bool emptySpaceBeforeOb) {
    using WriteVar = SimpleJsonLoader::WriteVar;
    auto func = [&](auto &&v) {
        //str.append(valueLayer, '\t');
        auto a = vstd::to_string(v);
        vstd::to_string(v, str);
    };
    switch (v.value.index()) {
        case WriteVar::IndexOf<int64>:
            func(eastl::get<int64>(v.value));
            break;
        case WriteVar::IndexOf<double>:
            func(eastl::get<double>(v.value));
            break;
        case WriteVar::IndexOf<vstd::string>:
            [&](vstd::string const &s) {
                //str.append(valueLayer, '\t');
                auto &&ss = str;
                PrintString(s, str);
            }(eastl::get<vstd::string>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonDict>>:
            [&](vstd::unique_ptr<IJsonDict> const &ptr) {
                if (emptySpaceBeforeOb)
                    str << '\n';
                auto &&ss = str;
                static_cast<Dict *>(ptr.get())->M_Print(str);
            }(eastl::get<vstd::unique_ptr<IJsonDict>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonArray>>:
            [&](vstd::unique_ptr<IJsonArray> const &ptr) {
                auto &&ssr = str;
                if (emptySpaceBeforeOb)
                    str << '\n';
                auto &&ss = str;
                static_cast<Array *>(ptr.get())->M_Print(str);
            }(eastl::get<vstd::unique_ptr<IJsonArray>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::Guid>:
            [&](vstd::Guid const &guid) {
                //str.append(valueLayer, '\t');
                str << '$';
                size_t offst = str.size();
                str.resize(offst + 32);
                guid.ToString(str.data() + offst, true);
            }(eastl::get<vstd::Guid>(v.value));
            break;
        case WriteVar::IndexOf<bool>:
            if (eastl::get<bool>(v.value))
                str += "true";
            else
                str += "false";
            break;
        default:
            str += "null";
            break;
    }
}
template<typename Dict, typename Array>
static void CompressPrintSimpleJsonVariant(SimpleJsonVariant const &v, vstd::string &str) {
    using WriteVar = SimpleJsonLoader::WriteVar;
    auto func = [&](auto &&v) {
        vstd::to_string(v, str);
    };
    switch (v.value.index()) {
        case WriteVar::IndexOf<int64>:
            func(eastl::get<int64>(v.value));
            break;
        case WriteVar::IndexOf<double>:
            func(eastl::get<double>(v.value));
            break;
        case WriteVar::IndexOf<vstd::string>:
            [&](vstd::string const &s) {
                PrintString(s, str);
            }(eastl::get<vstd::string>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonDict>>:
            [&](vstd::unique_ptr<IJsonDict> const &ptr) {
                static_cast<Dict *>(ptr.get())->M_Print_Compress(str);
            }(eastl::get<vstd::unique_ptr<IJsonDict>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonArray>>:
            [&](vstd::unique_ptr<IJsonArray> const &ptr) {
                static_cast<Array *>(ptr.get())->M_Print_Compress(str);
            }(eastl::get<vstd::unique_ptr<IJsonArray>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::Guid>:
            [&](vstd::Guid const &guid) {
                str << '$';
                size_t offst = str.size();
                str.resize(offst + 32);
                guid.ToString(str.data() + offst, true);
            }(eastl::get<vstd::Guid>(v.value));
            break;
        case WriteVar::IndexOf<bool>:
            if (eastl::get<bool>(v.value))
                str += "true";
            else
                str += "false";
            break;
        case WriteVar::IndexOf<std::nullptr_t>:
            str += "null";
            break;
    }
}
template<typename Dict, typename Array>
static void PrintSimpleJsonVariantYaml(SimpleJsonVariant const &v, vstd::string &str, size_t space) {
    using WriteVar = SimpleJsonLoader::WriteVar;
    auto func = [&](auto &&v) {
        vstd::to_string(v, str);
    };
    switch (v.value.index()) {
        case WriteVar::IndexOf<int64>:
            func(eastl::get<int64>(v.value));
            break;
        case WriteVar::IndexOf<double>:
            func(eastl::get<double>(v.value));
            break;
        case WriteVar::IndexOf<vstd::string>:
            [&](vstd::string const &s) {
                PrintString(s, str);
            }(eastl::get<vstd::string>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonDict>>:
            [&](vstd::unique_ptr<IJsonDict> const &ptr) {
                static_cast<Dict *>(ptr.get())->PrintYaml(str, space + 2);
            }(eastl::get<vstd::unique_ptr<IJsonDict>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::unique_ptr<IJsonArray>>:
            [&](vstd::unique_ptr<IJsonArray> const &ptr) {
                static_cast<Array *>(ptr.get())->PrintYaml(str, space + 2);
            }(eastl::get<vstd::unique_ptr<IJsonArray>>(v.value));
            break;
        case WriteVar::IndexOf<vstd::Guid>:
            [&](vstd::Guid const &guid) {
                size_t offst = str.size();
                str.resize(offst + 32);
                guid.ToString(str.data() + offst, true);
            }(eastl::get<vstd::Guid>(v.value));
            break;
        case WriteVar::IndexOf<bool>:
            if (eastl::get<bool>(v.value))
                str += "true";
            else
                str += "false";
            break;
        default:
            str += "null";
            break;
    }
}

static void PrintKeyVariant(SimpleJsonKey const &v, vstd::string &str) {
    auto func = [&](auto &&v) {
        vstd::to_string(v, str);
    };
    switch (v.value.index()) {
        case SimpleJsonKey::VarType::IndexOf<int64>:
            vstd::to_string(eastl::get<int64>(v.value), str);
            break;
        case SimpleJsonKey::VarType::IndexOf<vstd::string>:
            PrintString(eastl::get<vstd::string>(v.value), str);
            break;
        case SimpleJsonKey::VarType::IndexOf<vstd::Guid>:
            [&](vstd::Guid const &guid) {
                str << '$';
                size_t offst = str.size();
                str.resize(offst + 32);
                guid.ToString(str.data() + offst, true);
            }(eastl::get<vstd::Guid>(v.value));
            break;
    }
}
template<typename Dict, typename Array>
static void PrintDict(KVMap const &vars, vstd::string &str) {
    //str.append(space, '\t');
    str << "{\n"sv;
    auto disp = vstd::create_disposer([&]() {
        //str.append(space, '\t');
        str << '}';
    });
    size_t varsSize = vars.size() - 1;
    size_t index = 0;
    for (auto &&i : vars) {
        //str.append(space, '\t');
        PrintKeyVariant(i.first, str);
        str << ": "sv;
        PrintSimpleJsonVariant<Dict, Array>(i.second, str, true);
        if (index == varsSize) {
            str << '\n';
        } else {
            str << ",\n"sv;
        }
        index++;
    }
}
template<typename Dict, typename Array>
static void CompressPrintDict(KVMap const &vars, vstd::string &str) {
    str << '{';
    auto disp = vstd::create_disposer([&]() {
        str << '}';
    });
    size_t varsSize = vars.size() - 1;
    size_t index = 0;
    for (auto &&i : vars) {
        PrintKeyVariant(i.first, str);
        str << ':';
        CompressPrintSimpleJsonVariant<Dict, Array>(i.second, str);
        if (index != varsSize) {
            str << ',';
        }
        index++;
    }
}
template<typename Vector, typename Dict, typename Array>
static void PrintArray(Vector const &arr, vstd::string &str) {
    //str.append(space, '\t');
    str << "[\n"sv;
    auto disp = vstd::create_disposer([&]() {
        //str.append(space, '\t');
        str << ']';
    });
    size_t arrSize = arr.size() - 1;
    size_t index = 0;
    for (auto &&i : arr) {
        PrintSimpleJsonVariant<Dict, Array>(i, str, false);
        if (index == arrSize) {
            str << '\n';
        } else {
            str << ",\n"sv;
        }
        index++;
    }
}
template<typename Vector, typename Dict, typename Array>
static void CompressPrintArray(Vector const &arr, vstd::string &str) {
    str << '[';
    auto disp = vstd::create_disposer([&]() {
        str << ']';
    });
    size_t arrSize = arr.size() - 1;
    size_t index = 0;
    for (auto &&i : arr) {
        CompressPrintSimpleJsonVariant<Dict, Array>(i, str);
        if (index != arrSize) {
            str << ',';
        }
        index++;
    }
}
//////////////////////////  Single Thread
SimpleJsonValueDict::SimpleJsonValueDict(SimpleBinaryJson *db)
    : vars(4) {
    this->db = db;
}
SimpleJsonValueDict::~SimpleJsonValueDict() {
}
bool SimpleJsonValueDict::Contains(Key const &key) const {
    return vars.Find(key);
}
ReadJsonVariant SimpleJsonValueDict::Get(Key const &key) const {
    auto ite = vars.Find(key);
    if (ite) {
        return ite.Value().GetVariant();
    }
    return {};
}
void SimpleJsonValueDict::Set(Key const &key, WriteJsonVariant &&value) {
    vars.ForceEmplace(key, std::move(value));
}
ReadJsonVariant SimpleJsonValueDict::TrySet(Key const &key, vstd::function<WriteJsonVariant()> const &value) {
    return vars.Emplace(key, vstd::MakeLazyEval(value)).Value().GetVariant();
}

void SimpleJsonValueDict::TryReplace(Key const &key, vstd::function<WriteJsonVariant(ReadJsonVariant const &)> const &value) {
    auto ite = vars.Find(key);
    if (!ite) return;
    auto &&v = ite.Value();
    v = value(v.GetVariant());
}
void SimpleJsonValueDict::Remove(Key const &key) {
    vars.Remove(key);
}
eastl::optional<WriteJsonVariant> SimpleJsonValueDict::GetAndRemove(Key const &key) {
    auto ite = vars.Find(key);
    if (!ite) return {};
    auto d = vstd::create_disposer([&] {
        vars.Remove(ite);
    });
    return std::move(ite.Value().value);
}
eastl::optional<WriteJsonVariant> SimpleJsonValueDict::GetAndSet(Key const &key, WriteJsonVariant &&newValue) {
    auto ite = vars.Find(key);
    if (!ite) {
        vars.Emplace(key, std::move(newValue));
        return {};
    }
    auto d = vstd::create_disposer([&] {
        ite.Value() = std::move(newValue);
    });
    return std::move(ite.Value().value);
}

size_t SimpleJsonValueDict::Length() const {
    return vars.size();
}
vstd::vector<uint8_t> SimpleJsonValueDict::Serialize() const {
    vstd::vector<uint8_t> result;
    result.emplace_back(IsMachineLittleEnding());
    SerPreProcess(result);
    M_GetSerData(result);
    SerPostProcess<true>(result, 1);
    return result;
}
void SimpleJsonValueDict::Serialize(vstd::vector<uint8_t> &result) const {
    result.emplace_back(IsMachineLittleEnding());
    auto sz = result.size();
    SerPreProcess(result);
    M_GetSerData(result);
    SerPostProcess<true>(result, sz);
}
void SimpleJsonValueDict::M_GetSerData(vstd::vector<uint8_t> &data) const {
    PushDataToVector<uint64>(vars.size(), data);
    for (auto &&kv : vars) {
        PushDataToVector(kv.first.value, data);
        SimpleJsonLoader::Serialize(kv.second, data);
    }
}

void SimpleJsonValueDict::LoadFromSer(vstd::span<uint8_t const> &sp) {
    auto sz = PopValue<uint64>(sp);
    vars.reserve(sz);
    for (auto i : vstd::range(sz)) {
        auto key = PopValue<SimpleJsonKey::ValueType>(sp);

        auto value = SimpleJsonLoader::DeSerialize(sp, db);
        if (key.index() == 1) {
            auto &&s = eastl::get<vstd::string>(key);
            int x = 0;
        }
        vars.Emplace(std::move(key), std::move(value));
    }
}
void SimpleJsonValueDict::LoadFromSer_DiffEnding(vstd::span<uint8_t const> &sp) {
    auto sz = PopValueReverse<uint64>(sp);
    vars.reserve(sz);
    for (auto i : vstd::range(sz)) {
        auto key = PopValueReverse<SimpleJsonKey::ValueType>(sp);
        auto value = SimpleJsonLoader::DeSerialize_DiffEnding(sp, db);
        vars.Emplace(std::move(key), std::move(value));
    }
}

void SimpleJsonValueDict::Reserve(size_t capacity) {
    vars.reserve(capacity);
}
void SimpleJsonValueArray::Reserve(size_t capacity) {
    arr.reserve(capacity);
}

void SimpleJsonValueDict::Reset() {
    vars.Clear();
}

void SimpleJsonValueDict::Dispose() {
    db->dictValuePool.Delete_Lock(db->dictMtx, this);
}
void SimpleJsonValueArray::Dispose() {
    db->arrValuePool.Delete_Lock(db->arrMtx, this);
}

SimpleJsonValueArray::SimpleJsonValueArray(
    SimpleBinaryJson *db) {
    this->db = db;
}
SimpleJsonValueArray::~SimpleJsonValueArray() {
}

size_t SimpleJsonValueArray::Length() const {
    return arr.size();
}

vstd::vector<uint8_t> SimpleJsonValueArray::Serialize() const {
    vstd::vector<uint8_t> result;
    result.emplace_back(IsMachineLittleEnding());
    SerPreProcess(result);
    M_GetSerData(result);
    SerPostProcess<false>(result, 1);
    return result;
}
void SimpleJsonValueArray::Serialize(vstd::vector<uint8_t> &result) const {
    result.emplace_back(IsMachineLittleEnding());
    auto sz = result.size();
    SerPreProcess(result);
    M_GetSerData(result);
    SerPostProcess<false>(result, sz);
}

void SimpleJsonValueArray::M_GetSerData(vstd::vector<uint8_t> &data) const {
    PushDataToVector<uint64>(arr.size(), data);
    for (auto &&v : arr) {
        SimpleJsonLoader::Serialize(v, data);
    }
}

void SimpleJsonValueArray::LoadFromSer(vstd::span<uint8_t const> &sp) {
    auto sz = PopValue<uint64>(sp);

    arr.reserve(sz);
    for (auto i : vstd::range(sz)) {
        arr.emplace_back(SimpleJsonLoader::DeSerialize(sp, db));
    }
}
void SimpleJsonValueArray::LoadFromSer_DiffEnding(vstd::span<uint8_t const> &sp) {
    auto sz = PopValueReverse<uint64>(sp);
    arr.reserve(sz);
    for (auto i : vstd::range(sz)) {
        arr.emplace_back(SimpleJsonLoader::DeSerialize_DiffEnding(sp, db));
    }
}

void SimpleJsonValueArray::Reset() {
    arr.clear();
}

ReadJsonVariant SimpleJsonValueArray::Get(size_t index) const {
    if (index >= arr.size())
        return {};
    return arr[index].GetVariant();
}

void SimpleJsonValueArray::Set(size_t index, WriteJsonVariant &&value) {
    if (index < arr.size()) {
        arr[index].Set(std::move(value));
    }
}

void SimpleJsonValueArray::Remove(size_t index) {
    if (index < arr.size()) {
        arr.erase(arr.begin() + index);
    }
}
eastl::optional<WriteJsonVariant> SimpleJsonValueArray::GetAndRemove(size_t index) {
    if (index >= arr.size()) return {};
    auto d = vstd::create_disposer([&] {
        arr.erase(arr.begin() + index);
    });
    return std::move(arr[index].value);
}
eastl::optional<WriteJsonVariant> SimpleJsonValueArray::GetAndSet(size_t index, WriteJsonVariant &&newValue) {
    if (index >= arr.size()) return {};
    auto d = vstd::create_disposer([&] {
        arr.erase(arr.begin() + index);
    });
    return std::move(arr[index].value);
}

void SimpleJsonValueArray::Add(WriteJsonVariant &&value) {
    arr.emplace_back(std::move(value));
}
void SimpleJsonValueDict::M_Print(vstd::string &str) const {
    PrintDict<SimpleJsonValueDict, SimpleJsonValueArray>(vars, str);
}
void SimpleJsonValueArray::M_Print(vstd::string &str) const {
    PrintArray<decltype(arr), SimpleJsonValueDict, SimpleJsonValueArray>(arr, str);
}
void SimpleJsonValueDict::M_Print_Compress(vstd::string &str) const {
    CompressPrintDict<SimpleJsonValueDict, SimpleJsonValueArray>(vars, str);
}
void SimpleJsonValueArray::M_Print_Compress(vstd::string &str) const {
    CompressPrintArray<decltype(arr), SimpleJsonValueDict, SimpleJsonValueArray>(arr, str);
}
vstd::MD5 SimpleJsonValueDict::GetMD5() const {
    vstd::vector<uint8_t> vec;
    M_GetSerData(vec);
    return vstd::MD5(vec);
}
vstd::MD5 SimpleJsonValueArray::GetMD5() const {
    vstd::vector<uint8_t> vec;
    M_GetSerData(vec);
    return vstd::MD5(vec);
}
bool SimpleJsonValueDict::Read(vstd::span<uint8_t const> sp, bool clearLast) {
    bool sameEnding;
    if (!DeserCheck<true>(sp, sameEnding)) return false;
    if (clearLast) {
        vars.Clear();
    }
    if (sameEnding) {
        LoadFromSer(sp);
    } else {
        LoadFromSer_DiffEnding(sp);
    }
    return true;
}

bool SimpleJsonValueArray::Read(vstd::span<uint8_t const> sp, bool clearLast) {
    bool sameEnding;
    if (!DeserCheck<false>(sp, sameEnding)) return false;
    if (clearLast) {
        arr.clear();
    }
    if (sameEnding) {
        LoadFromSer(sp);
    } else {
        LoadFromSer_DiffEnding(sp);
    }
    return true;
}
vstd::Iterator<JsonKeyPair> SimpleJsonValueDict::begin() const & {
    return [&](void *ptr) { return new (ptr) DictIEnumerator(vars.begin(), vars.end()); };
}

vstd::Iterator<ReadJsonVariant> SimpleJsonValueArray::begin() const & {
    return [&](void *ptr) { return new (ptr) ArrayIEnumerator(arr.begin(), arr.end()); };
}
vstd::Iterator<MoveJsonKeyPair> SimpleJsonValueDict::begin() && {

    return [&](void *ptr) {
		auto deleterFunc = [&] { vars.Clear(); };
		return new (ptr) MoveDictIEnumerator<decltype(deleterFunc)>(std::move(deleterFunc), vars.begin(), vars.end()); };
}

vstd::Iterator<WriteJsonVariant> SimpleJsonValueArray::begin() && {
    return [&](void *ptr) {
		auto deleterFunc = [&] { arr.clear(); };
		return new (ptr) MoveArrayIEnumerator<decltype(deleterFunc)>(std::move(deleterFunc), arr.begin(), arr.end()); };
}
IJsonDatabase *SimpleJsonValueDict::GetDB() const { return db; }
IJsonDatabase *SimpleJsonValueArray::GetDB() const { return db; }
vstd::vector<ReadJsonVariant> SimpleJsonValueDict::Get(vstd::span<Key> keys) const {
    vstd::vector<ReadJsonVariant> vec(keys.size());
    for (auto i : vstd::range(keys.size())) {
        vec[i] = Get(keys[i]);
    }
    return vec;
}
vstd::vector<bool> SimpleJsonValueDict::Contains(vstd::span<Key> keys) const {
    vstd::vector<bool> vec(keys.size());
    for (auto i : vstd::range(keys.size())) {
        vec[i] = Contains(keys[i]);
    }
    return vec;
}
void SimpleJsonValueDict::Set(vstd::span<std::pair<Key, WriteJsonVariant>> kv) {
    for (auto &&i : kv) {
        Set(std::move(i.first), std::move(i.second));
    }
}
void SimpleJsonValueDict::Remove(vstd::span<Key> keys) {
    for (auto &&i : keys) {
        Remove(i);
    }
}
vstd::vector<std::pair<Key, ReadJsonVariant>> SimpleJsonValueDict::ToVector() const {
    vstd::vector<std::pair<Key, ReadJsonVariant>> vec;
    vec.reserve(vars.size());
    for (auto &&i : vars) {
        vec.emplace_back(i.first.GetKey(), i.second.GetVariant());
    }
    return vec;
}
vstd::vector<ReadJsonVariant> SimpleJsonValueArray::Get(vstd::span<size_t> indices) const {
    vstd::vector<ReadJsonVariant> vec(indices.size());
    for (auto i : vstd::range(indices.size())) {
        vec[i] = Get(indices[i]);
    }
    return vec;
}
void SimpleJsonValueArray::Set(vstd::span<std::pair<size_t, WriteJsonVariant>> values) {
    for (auto &&i : values) {
        Set(i.first, std::move(i.second));
    }
}
void SimpleJsonValueArray::Remove(vstd::span<size_t> indices) {
    for (auto &&i : indices) {
        Remove(i);
    }
}
void SimpleJsonValueArray::Add(vstd::span<WriteJsonVariant> values) {
    arr.reserve(arr.size() + values.size());
    for (auto &&i : values) {
        arr.emplace_back(std::move(i));
    }
}
vstd::vector<ReadJsonVariant> SimpleJsonValueArray::ToVector() const {
    vstd::vector<ReadJsonVariant> vec(arr.size());
    for (auto i : vstd::range(arr.size())) {
        vec[i] = arr[i].GetVariant();
    }
    return vec;
}

}// namespace toolhub::db