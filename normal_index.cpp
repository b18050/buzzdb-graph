#include <cassert>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>

#include <list>
#include <unordered_map>
#include <iostream>
#include <map>
#include <string>
#include <memory>
#include <sstream>
#include <limits>
#include <thread>
#include <queue>
#include <optional>
#include <random>
#include <unordered_set>

enum FieldType { INT, FLOAT, STRING };

// Define a basic Field variant class that can hold different types
class Field {
public:
    FieldType type;
    size_t data_length;
    std::unique_ptr<char[]> data;

public:
    Field(int i) : type(INT) {
        data_length = sizeof(int);
        data = std::make_unique<char[]>(data_length);
        std::memcpy(data.get(), &i, data_length);
    }

    Field(float f) : type(FLOAT) {
        data_length = sizeof(float);
        data = std::make_unique<char[]>(data_length);
        std::memcpy(data.get(), &f, data_length);
    }

    Field(const std::string& s) : type(STRING) {
        data_length = s.size() + 1;  // include null-terminator
        data = std::make_unique<char[]>(data_length);
        std::memcpy(data.get(), s.c_str(), data_length);
    }

    Field& operator=(const Field& other) {
        if (&other == this) {
            return *this;
        }
        type = other.type;
        data_length = other.data_length;
        std::memcpy(data.get(), other.data.get(), data_length);
        return *this;
    }

   // Copy constructor
    Field(const Field& other) : type(other.type), data_length(other.data_length), data(new char[data_length]) {
        std::memcpy(data.get(), other.data.get(), data_length);
    }

    // Move constructor - If you already have one, ensure it's correctly implemented
    Field(Field&& other) noexcept : type(other.type), data_length(other.data_length), data(std::move(other.data)) {
        // Optionally reset other's state if needed
    }

    // Clone method
    std::unique_ptr<Field> clone() const {
        // Use the copy constructor
        return std::make_unique<Field>(*this);
    }

    FieldType getType() const { return type; }
    int asInt() const {
        return *reinterpret_cast<int*>(data.get());
    }
    float asFloat() const {
        return *reinterpret_cast<float*>(data.get());
    }
    std::string asString() const {
        return std::string(data.get());
    }

    std::string serialize() {
        std::stringstream buffer;
        buffer << type << ' ' << data_length << ' ';
        if (type == STRING) {
            buffer << data.get() << ' ';
        } else if (type == INT) {
            buffer << *reinterpret_cast<int*>(data.get()) << ' ';
        } else if (type == FLOAT) {
            buffer << *reinterpret_cast<float*>(data.get()) << ' ';
        }
        return buffer.str();
    }

    void serialize(std::ofstream& out) {
        std::string serializedData = this->serialize();
        out << serializedData;
    }

    static std::unique_ptr<Field> deserialize(std::istream& in) {
        int type; in >> type;
        size_t length; in >> length;
        if (type == STRING) {
            std::string val; in >> val;
            return std::make_unique<Field>(val);
        } else if (type == INT) {
            int val; in >> val;
            return std::make_unique<Field>(val);
        } else if (type == FLOAT) {
            float val; in >> val;
            return std::make_unique<Field>(val);
        }
        return nullptr;
    }

    void print() const{
        switch(getType()){
            case INT: std::cout << asInt(); break;
            case FLOAT: std::cout << asFloat(); break;
            case STRING: std::cout << asString(); break;
        }
    }
};

bool operator==(const Field& lhs, const Field& rhs) {
    if (lhs.type != rhs.type) return false; // Different types are never equal

    switch (lhs.type) {
        case INT:
            return *reinterpret_cast<const int*>(lhs.data.get()) == *reinterpret_cast<const int*>(rhs.data.get());
        case FLOAT:
            return *reinterpret_cast<const float*>(lhs.data.get()) == *reinterpret_cast<const float*>(rhs.data.get());
        case STRING:
            return std::string(lhs.data.get(), lhs.data_length - 1) == std::string(rhs.data.get(), rhs.data_length - 1);
        default:
            throw std::runtime_error("Unsupported field type for comparison.");
    }
}

class Tuple {
public:
    std::vector<std::unique_ptr<Field>> fields;

    void addField(std::unique_ptr<Field> field) {
        fields.push_back(std::move(field));
    }

    size_t getSize() const {
        size_t size = 0;
        for (const auto& field : fields) {
            size += field->data_length;
        }
        return size;
    }

    std::string serialize() {
        std::stringstream buffer;
        buffer << fields.size() << ' ';
        for (const auto& field : fields) {
            buffer << field->serialize();
        }
        return buffer.str();
    }

    void serialize(std::ofstream& out) {
        std::string serializedData = this->serialize();
        out << serializedData;
    }

    static std::unique_ptr<Tuple> deserialize(std::istream& in) {
        auto tuple = std::make_unique<Tuple>();
        size_t fieldCount; in >> fieldCount;
        for (size_t i = 0; i < fieldCount; ++i) {
            tuple->addField(Field::deserialize(in));
        }
        return tuple;
    }

    void print() const {
        for (const auto& field : fields) {
            field->print();
            std::cout << " ";
        }
        std::cout << "\n";
    }
};

static constexpr size_t PAGE_SIZE = 4096;  // Fixed page size
static constexpr size_t MAX_SLOTS = 512;   // Fixed number of slots
uint16_t INVALID_VALUE = std::numeric_limits<uint16_t>::max(); // Sentinel value

struct Slot {
    bool empty = true;                 // Is the slot empty?
    uint16_t offset = INVALID_VALUE;    // Offset of the slot within the page
    uint16_t length = INVALID_VALUE;    // Length of the slot
};

// Slotted Page class
class SlottedPage {
public:
    std::unique_ptr<char[]> page_data = std::make_unique<char[]>(PAGE_SIZE);
    size_t metadata_size = sizeof(Slot) * MAX_SLOTS;

    SlottedPage(){
        // Empty page -> initialize slot array inside page
        Slot* slot_array = reinterpret_cast<Slot*>(page_data.get());
        for (size_t slot_itr = 0; slot_itr < MAX_SLOTS; slot_itr++) {
            slot_array[slot_itr].empty = true;
            slot_array[slot_itr].offset = INVALID_VALUE;
            slot_array[slot_itr].length = INVALID_VALUE;
        }
    }

    // Add a tuple, returns true if it fits, false otherwise.
    bool addTuple(std::unique_ptr<Tuple> tuple) {

        // Serialize the tuple into a char array
        auto serializedTuple = tuple->serialize();
        size_t tuple_size = serializedTuple.size();

        //std::cout << "Tuple size: " << tuple_size << " bytes\n";
        // assert(tuple_size == 38);

        // Check for first slot with enough space
        size_t slot_itr = 0;
        Slot* slot_array = reinterpret_cast<Slot*>(page_data.get());
        for (; slot_itr < MAX_SLOTS; slot_itr++) {
            if (slot_array[slot_itr].empty == true and
                slot_array[slot_itr].length >= tuple_size) {
                break;
            }
        }
        if (slot_itr == MAX_SLOTS){
            //std::cout << "Page does not contain an empty slot with sufficient space to store the tuple.";
            return false;
        }

        // Identify the offset where the tuple will be placed in the page
        // Update slot meta-data if needed
        slot_array[slot_itr].empty = false;
        size_t offset = INVALID_VALUE;
        if (slot_array[slot_itr].offset == INVALID_VALUE){
            if(slot_itr != 0){
                auto prev_slot_offset = slot_array[slot_itr - 1].offset;
                auto prev_slot_length = slot_array[slot_itr - 1].length;
                offset = prev_slot_offset + prev_slot_length;
            }
            else{
                offset = metadata_size;
            }

            slot_array[slot_itr].offset = offset;
        }
        else{
            offset = slot_array[slot_itr].offset;
        }

        if(offset + tuple_size >= PAGE_SIZE){
            slot_array[slot_itr].empty = true;
            slot_array[slot_itr].offset = INVALID_VALUE;
            return false;
        }

        assert(offset != INVALID_VALUE);
        assert(offset >= metadata_size);
        assert(offset + tuple_size < PAGE_SIZE);

        if (slot_array[slot_itr].length == INVALID_VALUE){
            slot_array[slot_itr].length = tuple_size;
        }

        // Copy serialized data into the page
        std::memcpy(page_data.get() + offset,
                    serializedTuple.c_str(),
                    tuple_size);

        return true;
    }

    void deleteTuple(size_t index) {
        Slot* slot_array = reinterpret_cast<Slot*>(page_data.get());
        size_t slot_itr = 0;
        for (; slot_itr < MAX_SLOTS; slot_itr++) {
            if(slot_itr == index and
               slot_array[slot_itr].empty == false){
                slot_array[slot_itr].empty = true;
                break;
               }
        }

        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void print() const{
        Slot* slot_array = reinterpret_cast<Slot*>(page_data.get());
        for (size_t slot_itr = 0; slot_itr < MAX_SLOTS; slot_itr++) {
            if (slot_array[slot_itr].empty == false){
                assert(slot_array[slot_itr].offset != INVALID_VALUE);
                const char* tuple_data = page_data.get() + slot_array[slot_itr].offset;
                std::istringstream iss(tuple_data);
                auto loadedTuple = Tuple::deserialize(iss);
                std::cout << "Slot " << slot_itr << " : [";
                std::cout << (uint16_t)(slot_array[slot_itr].offset) << "] :: ";
                loadedTuple->print();
            }
        }
        std::cout << "\n";
    }
};

const std::string database_filename = "buzzdb.dat";

class StorageManager {
public:
    std::fstream fileStream;
    size_t num_pages = 0;

public:
    StorageManager(){
        fileStream.open(database_filename, std::ios::in | std::ios::out);
        if (!fileStream) {
            // If file does not exist, create it
            fileStream.clear(); // Reset the state
            fileStream.open(database_filename, std::ios::out);
        }
        fileStream.close();
        fileStream.open(database_filename, std::ios::in | std::ios::out);

        fileStream.seekg(0, std::ios::end);
        num_pages = fileStream.tellg() / PAGE_SIZE;

        std::cout << "Storage Manager :: Num pages: " << num_pages << "\n";
        if(num_pages == 0){
            extend();
        }

    }

    ~StorageManager() {
        if (fileStream.is_open()) {
            fileStream.close();
        }
    }

    // Read a page from disk
    std::unique_ptr<SlottedPage> load(uint16_t page_id) {
        fileStream.seekg(page_id * PAGE_SIZE, std::ios::beg);
        auto page = std::make_unique<SlottedPage>();
        // Read the content of the file into the page
        if(fileStream.read(page->page_data.get(), PAGE_SIZE)){
            //std::cout << "Page read successfully from file." << std::endl;
        }
        else{
            std::cerr << "Error: Unable to read data from the file. \n";
            exit(-1);
        }
        return page;
    }

    // Write a page to disk
    void flush(uint16_t page_id, const std::unique_ptr<SlottedPage>& page) {
        size_t page_offset = page_id * PAGE_SIZE;

        // Move the write pointer
        fileStream.seekp(page_offset, std::ios::beg);
        fileStream.write(page->page_data.get(), PAGE_SIZE);
        fileStream.flush();
    }

    // Extend database file by one page
    void extend() {
        std::cout << "Extending database file \n";

        // Create a slotted page
        auto empty_slotted_page = std::make_unique<SlottedPage>();

        // Move the write pointer
        fileStream.seekp(0, std::ios::end);

        // Write the page to the file, extending it
        fileStream.write(empty_slotted_page->page_data.get(), PAGE_SIZE);
        fileStream.flush();

        // Update number of pages
        num_pages += 1;
    }

};

using PageID = uint16_t;

class Policy {
public:
    virtual bool touch(PageID page_id) = 0;
    virtual PageID evict() = 0;
    virtual ~Policy() = default;
};

void printList(std::string list_name, const std::list<PageID>& myList) {
        std::cout << list_name << " :: ";
        for (const PageID& value : myList) {
            std::cout << value << ' ';
        }
        std::cout << '\n';
}

class LruPolicy : public Policy {
private:
    // List to keep track of the order of use
    std::list<PageID> lruList;

    // Map to find a page's iterator in the list efficiently
    std::unordered_map<PageID, std::list<PageID>::iterator> map;

    size_t cacheSize;

public:

    LruPolicy(size_t cacheSize) : cacheSize(cacheSize) {}

    bool touch(PageID page_id) override {
        //printList("LRU", lruList);

        bool found = false;
        // If page already in the list, remove it
        if (map.find(page_id) != map.end()) {
            found = true;
            lruList.erase(map[page_id]);
            map.erase(page_id);
        }

        // If cache is full, evict
        if(lruList.size() == cacheSize){
            evict();
        }

        if(lruList.size() < cacheSize){
            // Add the page to the front of the list
            lruList.emplace_front(page_id);
            map[page_id] = lruList.begin();
        }

        return found;
    }

    PageID evict() override {
        // Evict the least recently used page
        PageID evictedPageId = INVALID_VALUE;
        if(lruList.size() != 0){
            evictedPageId = lruList.back();
            map.erase(evictedPageId);
            lruList.pop_back();
        }
        return evictedPageId;
    }

};

constexpr size_t MAX_PAGES_IN_MEMORY = 10;

class BufferManager {
private:
    using PageMap = std::unordered_map<PageID, std::unique_ptr<SlottedPage>>;

    StorageManager storage_manager;
    PageMap pageMap;
    std::unique_ptr<Policy> policy;

public:
    BufferManager():
    policy(std::make_unique<LruPolicy>(MAX_PAGES_IN_MEMORY)) {}

    std::unique_ptr<SlottedPage>& getPage(int page_id) {
        auto it = pageMap.find(page_id);
        if (it != pageMap.end()) {
            policy->touch(page_id);
            return pageMap.find(page_id)->second;
        }

        if (pageMap.size() >= MAX_PAGES_IN_MEMORY) {
            auto evictedPageId = policy->evict();
            if(evictedPageId != INVALID_VALUE){
                std::cout << "Evicting page " << evictedPageId << "\n";
                storage_manager.flush(evictedPageId,
                                      pageMap[evictedPageId]);
            }
        }

        auto page = storage_manager.load(page_id);
        policy->touch(page_id);
        std::cout << "Loading page: " << page_id << "\n";
        pageMap[page_id] = std::move(page);
        return pageMap[page_id];
    }

    void flushPage(int page_id) {
        //std::cout << "Flush page " << page_id << "\n";
        storage_manager.flush(page_id, pageMap[page_id]);
    }

    void extend(){
        storage_manager.extend();
    }

    size_t getNumPages(){
        return storage_manager.num_pages;
    }

};

class HashIndex {
private:
    struct HashEntry {
        std::pair<int, int> key;
        std::pair<int, int> value;
        int position; // Final position within the array
        bool exists; // Flag to check if entry exists

        // Default constructor
        HashEntry() : key({0, 0}), value({0, 0}), position(-1), exists(false) {}

        // Constructor for initializing with key, value, and exists flag
        HashEntry(std::pair<int, int> k, std::pair<int, int> v, int pos) : key(std::move(k)), value(v), position(pos),
                                                                           exists(true) {
        }
    };

    static const size_t capacity = 10000; // Hard-coded capacity
    HashEntry hashTable[capacity]; // Static-sized array

    size_t hashFunction(const std::pair<int, int>& key) const {
        // Combine hash of both elements of the pair
        return (std::hash<int>()(key.first) ^ (std::hash<int>()(key.second) << 1)) % capacity;
    }

public:
    HashIndex() {
        // Initialize all entries as non-existing by default
        for (size_t i = 0; i < capacity; ++i) {
            hashTable[i] = HashEntry();
        }
    }

    void insertOrUpdate(std::pair<int, int> key, std::pair<int, int> value) {
        size_t index = hashFunction(key);
        size_t originalIndex = index;
        bool inserted = false;
        int i = 0; // Attempt counter

        do {
            if (!hashTable[index].exists) {
                hashTable[index] = HashEntry(key, value, true);
                hashTable[index].position = index;
                inserted = true;
                break;
            } else if (hashTable[index].key == key) {
                hashTable[index].value = value;
                hashTable[index].position = index;
                inserted = true;
                break;
            }
            i++;
            index = (originalIndex + i*i) % capacity; // Quadratic probing
        } while (index != originalIndex && !inserted);

        if (!inserted) {
            std::cerr << "HashTable is full or cannot insert key: " << key.first << ", " << key.second << std::endl;
        }
    }

   std::pair<int, int> getValue(std::pair<int, int> key) const {
        size_t index = hashFunction(key);
        size_t originalIndex = index;

        do {
            if (hashTable[index].exists && hashTable[index].key == key) {
                return hashTable[index].value;
            }
            if (!hashTable[index].exists) {
                break; // Stop if we find a slot that has never been used
            }
            index = (index + 1) % capacity;
        } while (index != originalIndex);

        return {-1, -1}; // Key not found
    }

    std::vector<std::pair<int, int>> findByFirstKey(int first) const {
        std::vector<std::pair<int, int>> results;
        for (size_t i = 0; i < capacity; ++i) {
            if (hashTable[i].exists && hashTable[i].key.first == first) {
                results.push_back(hashTable[i].value);
            }
        }
        return results;
    }

    std::vector<int> getFriendsOfAUser(int friend_id) const {
        std::vector<int> friends;
        std::vector<std::pair<int, int>> friendsPair = findByFirstKey(friend_id);
        for ( auto& friendPair : friendsPair) {
            friends.push_back(friendPair.second);
        }
        return friends;
    }

    void print() const {
        for (size_t i = 0; i < capacity; ++i) {
            if (hashTable[i].exists) {
                std::cout << "Position: " << hashTable[i].position <<
                ", Key: " << hashTable[i].key.first << " " << hashTable[i].key.second << " "
                ", Value: " << hashTable[i].value.first << " " << hashTable[i].value.second << std::endl;
            }
        }
    }
};


class Operator {
    public:
    virtual ~Operator() = default;

    /// Initializes the operator.
    virtual void open() = 0;

    /// Tries to generate the next tuple. Return true when a new tuple is
    /// available.
    virtual bool next() = 0;

    /// Destroys the operator.
    virtual void close() = 0;

    /// This returns the pointers to the Fields of the generated tuple. When
    /// `next()` returns true, the Fields will contain the values for the
    /// next tuple. Each `Field` pointer in the vector stands for one attribute of the tuple.
    virtual std::vector<std::unique_ptr<Field>> getOutput() = 0;
};

class UnaryOperator : public Operator {
    protected:
    Operator* input;

    public:
    explicit UnaryOperator(Operator& input) : input(&input) {}

    ~UnaryOperator() override = default;
};

class BinaryOperator : public Operator {
    protected:
    Operator* input_left;
    Operator* input_right;

    public:
    explicit BinaryOperator(Operator& input_left, Operator& input_right)
        : input_left(&input_left), input_right(&input_right) {}

    ~BinaryOperator() override = default;
};

class ScanOperator : public Operator {
private:
    BufferManager& bufferManager;
    size_t currentPageIndex = 0;
    size_t currentSlotIndex = 0;
    std::unique_ptr<Tuple> currentTuple;
    size_t tuple_count = 0;

public:
    ScanOperator(BufferManager& manager) : bufferManager(manager) {}

    void open() override {
        currentPageIndex = 0;
        currentSlotIndex = 0;
        currentTuple.reset(); // Ensure currentTuple is reset
        loadNextTuple();
    }

    bool next() override {
        if (!currentTuple) return false; // No more tuples available

        loadNextTuple();
        return currentTuple != nullptr;
    }

    void close() override {
        std::cout << "Scan Operator tuple_count: " << tuple_count << "\n";
        currentPageIndex = 0;
        currentSlotIndex = 0;
        currentTuple.reset();
    }

    std::vector<std::unique_ptr<Field>> getOutput() override {
        if (currentTuple) {
            return std::move(currentTuple->fields);
        }
        return {}; // Return an empty vector if no tuple is available
    }

private:
    void loadNextTuple() {
        while (currentPageIndex < bufferManager.getNumPages()) {
            auto& currentPage = bufferManager.getPage(currentPageIndex);
            if (!currentPage || currentSlotIndex >= MAX_SLOTS) {
                currentSlotIndex = 0; // Reset slot index when moving to a new page
            }

            char* page_buffer = currentPage->page_data.get();
            Slot* slot_array = reinterpret_cast<Slot*>(page_buffer);

            while (currentSlotIndex < MAX_SLOTS) {
                if (!slot_array[currentSlotIndex].empty) {
                    assert(slot_array[currentSlotIndex].offset != INVALID_VALUE);
                    const char* tuple_data = page_buffer + slot_array[currentSlotIndex].offset;
                    std::istringstream iss(std::string(tuple_data, slot_array[currentSlotIndex].length));
                    currentTuple = Tuple::deserialize(iss);
                    currentSlotIndex++; // Move to the next slot for the next call
                    tuple_count++;
                    return; // Tuple loaded successfully
                }
                currentSlotIndex++;
            }

            // Increment page index after exhausting current page
            currentPageIndex++;
        }

        // No more tuples are available
        currentTuple.reset();
    }
};


class IPredicate {
public:
    virtual ~IPredicate() = default;
    virtual bool check(const std::vector<std::unique_ptr<Field>>& tupleFields) const = 0;
};

void printTuple(const std::vector<std::unique_ptr<Field>>& tupleFields) {
    std::cout << "Tuple: [";
    for (const auto& field : tupleFields) {
        field->print(); // Assuming `print()` is a method that prints field content
        std::cout << " ";
    }
    std::cout << "]";
}

class SimplePredicate: public IPredicate {
public:
    enum OperandType { DIRECT, INDIRECT };
    enum ComparisonOperator { EQ, NE, GT, GE, LT, LE }; // Renamed from PredicateType

    struct Operand {
        std::unique_ptr<Field> directValue;
        size_t index;
        OperandType type;

        Operand(std::unique_ptr<Field> value) : directValue(std::move(value)), type(DIRECT) {}
        Operand(size_t idx) : index(idx), type(INDIRECT) {}
    };

    Operand left_operand;
    Operand right_operand;
    ComparisonOperator comparison_operator;

    SimplePredicate(Operand left, Operand right, ComparisonOperator op)
        : left_operand(std::move(left)), right_operand(std::move(right)), comparison_operator(op) {}

    bool check(const std::vector<std::unique_ptr<Field>>& tupleFields) const {
        const Field* leftField = nullptr;
        const Field* rightField = nullptr;

        if (left_operand.type == DIRECT) {
            leftField = left_operand.directValue.get();
        } else if (left_operand.type == INDIRECT) {
            leftField = tupleFields[left_operand.index].get();
        }

        if (right_operand.type == DIRECT) {
            rightField = right_operand.directValue.get();
        } else if (right_operand.type == INDIRECT) {
            rightField = tupleFields[right_operand.index].get();
        }

        if (leftField == nullptr || rightField == nullptr) {
            std::cerr << "Error: Invalid field reference.\n";
            return false;
        }

        if (leftField->getType() != rightField->getType()) {
            std::cerr << "Error: Comparing fields of different types.\n";
            return false;
        }

        // Perform comparison based on field type
        switch (leftField->getType()) {
            case FieldType::INT: {
                int left_val = leftField->asInt();
                int right_val = rightField->asInt();
                return compare(left_val, right_val);
            }
            case FieldType::FLOAT: {
                float left_val = leftField->asFloat();
                float right_val = rightField->asFloat();
                return compare(left_val, right_val);
            }
            case FieldType::STRING: {
                std::string left_val = leftField->asString();
                std::string right_val = rightField->asString();
                return compare(left_val, right_val);
            }
            default:
                std::cerr << "Invalid field type\n";
                return false;
        }
    }


private:

    // Compares two values of the same type
    template<typename T>
    bool compare(const T& left_val, const T& right_val) const {
        switch (comparison_operator) {
            case ComparisonOperator::EQ: return left_val == right_val;
            case ComparisonOperator::NE: return left_val != right_val;
            case ComparisonOperator::GT: return left_val > right_val;
            case ComparisonOperator::GE: return left_val >= right_val;
            case ComparisonOperator::LT: return left_val < right_val;
            case ComparisonOperator::LE: return left_val <= right_val;
            default: std::cerr << "Invalid predicate type\n"; return false;
        }
    }
};

class ComplexPredicate : public IPredicate {
public:
    enum LogicOperator { AND, OR };

private:
    std::vector<std::unique_ptr<IPredicate>> predicates;
    LogicOperator logic_operator;

public:
    ComplexPredicate(LogicOperator op) : logic_operator(op) {}

    void addPredicate(std::unique_ptr<IPredicate> predicate) {
        predicates.push_back(std::move(predicate));
    }

    bool check(const std::vector<std::unique_ptr<Field>>& tupleFields) const {

        if (logic_operator == AND) {
            for (const auto& pred : predicates) {
                if (!pred->check(tupleFields)) {
                    return false; // If any predicate fails, the AND condition fails
                }
            }
            return true; // All predicates passed
        } else if (logic_operator == OR) {
            for (const auto& pred : predicates) {
                if (pred->check(tupleFields)) {
                    return true; // If any predicate passes, the OR condition passes
                }
            }
            return false; // No predicates passed
        }
        return false;
    }


};


class SelectOperator : public UnaryOperator {
private:
    std::unique_ptr<IPredicate> predicate;
    bool has_next;
    std::vector<std::unique_ptr<Field>> currentOutput; // Store the current output here

public:
    SelectOperator(Operator& input, std::unique_ptr<IPredicate> predicate)
        : UnaryOperator(input), predicate(std::move(predicate)), has_next(false) {}

    void open() override {
        input->open();
        has_next = false;
        currentOutput.clear(); // Ensure currentOutput is cleared at the beginning
    }

    bool next() override {
        while (input->next()) {
            const auto& output = input->getOutput(); // Temporarily hold the output
            if (predicate->check(output)) {
                // If the predicate is satisfied, store the output in the member variable
                currentOutput.clear(); // Clear previous output
                for (const auto& field : output) {
                    // Assuming Field class has a clone method or copy constructor to duplicate fields
                    currentOutput.push_back(field->clone());
                }
                has_next = true;
                return true;
            }
        }
        has_next = false;
        currentOutput.clear(); // Clear output if no more tuples satisfy the predicate
        return false;
    }

    void close() override {
        input->close();
        currentOutput.clear(); // Ensure currentOutput is cleared at the end
    }

    std::vector<std::unique_ptr<Field>> getOutput() override {
        if (has_next) {
            // Since currentOutput already holds the desired output, simply return it
            // Need to create a deep copy to return since we're returning by value
            std::vector<std::unique_ptr<Field>> outputCopy;
            for (const auto& field : currentOutput) {
                outputCopy.push_back(field->clone()); // Clone each field
            }
            return outputCopy;
        } else {
            return {}; // Return an empty vector if no matching tuple is found
        }
    }
};

enum class AggrFuncType { COUNT, MAX, MIN, SUM };

struct AggrFunc {
    AggrFuncType func;
    size_t attr_index; // Index of the attribute to aggregate
};

class HashAggregationOperator : public UnaryOperator {
private:
    std::vector<size_t> group_by_attrs;
    std::vector<AggrFunc> aggr_funcs;
    std::vector<Tuple> output_tuples; // Use your Tuple class for output
    size_t output_tuples_index = 0;

    struct FieldVectorHasher {
        std::size_t operator()(const std::vector<Field>& fields) const {
            std::size_t hash = 0;
            for (const auto& field : fields) {
                std::hash<std::string> hasher;
                std::size_t fieldHash = 0;

                // Depending on the type, hash the corresponding data
                switch (field.type) {
                    case INT: {
                        // Convert integer data to string and hash
                        int value = *reinterpret_cast<const int*>(field.data.get());
                        fieldHash = hasher(std::to_string(value));
                        break;
                    }
                    case FLOAT: {
                        // Convert float data to string and hash
                        float value = *reinterpret_cast<const float*>(field.data.get());
                        fieldHash = hasher(std::to_string(value));
                        break;
                    }
                    case STRING: {
                        // Directly hash the string data
                        std::string value(field.data.get(), field.data_length - 1); // Exclude null-terminator
                        fieldHash = hasher(value);
                        break;
                    }
                    default:
                        throw std::runtime_error("Unsupported field type for hashing.");
                }

                // Combine the hash of the current field with the hash so far
                hash ^= fieldHash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };


public:
    HashAggregationOperator(Operator& input, std::vector<size_t> group_by_attrs, std::vector<AggrFunc> aggr_funcs)
        : UnaryOperator(input), group_by_attrs(group_by_attrs), aggr_funcs(aggr_funcs) {}

    void open() override {
        input->open(); // Ensure the input operator is opened
        output_tuples_index = 0;
        output_tuples.clear();

        // Assume a hash map to aggregate tuples based on group_by_attrs
        std::unordered_map<std::vector<Field>, std::vector<Field>, FieldVectorHasher> hash_table;

        while (input->next()) {
            const auto& tuple = input->getOutput(); // Assume getOutput returns a reference to the current tuple

            // Extract group keys and initialize aggregation values
            std::vector<Field> group_keys;
            for (auto& index : group_by_attrs) {
                group_keys.push_back(*tuple[index]); // Deep copy the Field object for group key
            }

            // Process aggregation functions
            if (!hash_table.count(group_keys)) {
                // Initialize aggregate values for a new group
                std::vector<Field> aggr_values(aggr_funcs.size(), Field(0)); // Assuming Field(int) initializes an integer Field
                hash_table[group_keys] = aggr_values;
            }

            // Update aggregate values
            auto& aggr_values = hash_table[group_keys];
            for (size_t i = 0; i < aggr_funcs.size(); ++i) {
                // Simplified update logic for demonstration
                // You'll need to implement actual aggregation logic here
                aggr_values[i] = updateAggregate(aggr_funcs[i], aggr_values[i], *tuple[aggr_funcs[i].attr_index]);
            }
        }

        // Prepare output tuples from the hash table
        for (const auto& entry : hash_table) {
            const auto& group_keys = entry.first;
            const auto& aggr_values = entry.second;
            Tuple output_tuple;
            // Assuming Tuple has a method to add Fields
            for (const auto& key : group_keys) {
                output_tuple.addField(std::make_unique<Field>(key)); // Add group keys to the tuple
            }
            for (const auto& value : aggr_values) {
                output_tuple.addField(std::make_unique<Field>(value)); // Add aggregated values to the tuple
            }
            output_tuples.push_back(std::move(output_tuple));
        }
    }

    bool next() override {
        if (output_tuples_index < output_tuples.size()) {
            output_tuples_index++;
            return true;
        }
        return false;
    }

    void close() override {
        input->close();
    }

    std::vector<std::unique_ptr<Field>> getOutput() override {
        std::vector<std::unique_ptr<Field>> outputCopy;

        if (output_tuples_index == 0 || output_tuples_index > output_tuples.size()) {
            // If there is no current tuple because next() hasn't been called yet or we're past the last tuple,
            // return an empty vector.
            return outputCopy; // This will be an empty vector
        }

        // Assuming that output_tuples stores Tuple objects and each Tuple has a vector of Field objects or similar
        const auto& currentTuple = output_tuples[output_tuples_index - 1]; // Adjust for 0-based indexing after increment in next()

        // Assuming the Tuple class provides a way to access its fields, e.g., a method or a public member
        for (const auto& field : currentTuple.fields) {
            outputCopy.push_back(field->clone()); // Use the clone method to create a deep copy of each field
        }

        return outputCopy;
    }


private:

    Field updateAggregate(const AggrFunc& aggrFunc, const Field& currentAggr, const Field& newValue) {
        if (currentAggr.getType() != newValue.getType()) {
            throw std::runtime_error("Mismatched Field types in aggregation.");
        }

        switch (aggrFunc.func) {
            case AggrFuncType::COUNT: {
                if (currentAggr.getType() == FieldType::INT) {
                    // For COUNT, simply increment the integer value
                    int count = currentAggr.asInt() + 1;
                    return Field(count);
                }
                break;
            }
            case AggrFuncType::SUM: {
                if (currentAggr.getType() == FieldType::INT) {
                    int sum = currentAggr.asInt() + newValue.asInt();
                    return Field(sum);
                } else if (currentAggr.getType() == FieldType::FLOAT) {
                    float sum = currentAggr.asFloat() + newValue.asFloat();
                    return Field(sum);
                }
                break;
            }
            case AggrFuncType::MAX: {
                if (currentAggr.getType() == FieldType::INT) {
                    int max = std::max(currentAggr.asInt(), newValue.asInt());
                    return Field(max);
                } else if (currentAggr.getType() == FieldType::FLOAT) {
                    float max = std::max(currentAggr.asFloat(), newValue.asFloat());
                    return Field(max);
                }
                break;
            }
            case AggrFuncType::MIN: {
                if (currentAggr.getType() == FieldType::INT) {
                    int min = std::min(currentAggr.asInt(), newValue.asInt());
                    return Field(min);
                } else if (currentAggr.getType() == FieldType::FLOAT) {
                    float min = std::min(currentAggr.asFloat(), newValue.asFloat());
                    return Field(min);
                }
                break;
            }
            default:
                throw std::runtime_error("Unsupported aggregation function.");
        }

        // Default case for unsupported operations or types
        throw std::runtime_error(
            "Invalid operation or unsupported Field type.");
    }

};



std::vector<int> findMutualFriends(int user_id1, int user_id2, const HashIndex& hash_index) {
    auto friends1 = hash_index.getFriendsOfAUser(user_id1);
    auto friends2 = hash_index.getFriendsOfAUser(user_id2);

    std::unordered_set<int> set_friends1(friends1.begin(), friends1.end());
    std::vector<int> mutual_friends;
    mutual_friends.clear();

    for (int friend_id : friends2) {
        if (set_friends1.find(friend_id) != set_friends1.end() && friend_id != user_id1 && friend_id != user_id2) {
            mutual_friends.push_back(friend_id);
        }
    }
    return mutual_friends;
}

std::vector<int> findSecondLevelMutualFriends(int user_id1, int user_id2, const HashIndex& hash_index) {

    auto firstLevelMutualFriends = findMutualFriends(user_id1, user_id2, hash_index);

    std::unordered_set<int> secondLevelSet;
    std::unordered_set<int> firstLevelSet(firstLevelMutualFriends.begin(), firstLevelMutualFriends.end());


    for (int mutualFriend : firstLevelMutualFriends) {
        auto friendsOfMutual = hash_index.getFriendsOfAUser(mutualFriend);

        for (int friend_id : friendsOfMutual) {
            if (friend_id != user_id1 && friend_id != user_id2 && firstLevelSet.find(friend_id) == firstLevelSet.end()) {
                secondLevelSet.insert(friend_id); // Insert to ensure uniqueness
            }
        }
    }

    std::vector<int> secondLevelMutualFriends(secondLevelSet.begin(), secondLevelSet.end());
    return secondLevelMutualFriends;
}

std::vector<int> getFriends(int user_id, const HashIndex& hash_index) {
    return hash_index.getFriendsOfAUser(user_id);
}

class BuzzDB {
public:
    HashIndex hash_index;
    BufferManager buffer_manager;

public:

    size_t max_number_of_tuples = 5000;
    size_t tuple_insertion_attempt_counter = 0;
    BuzzDB() {
        // Initialization complete
    }


    bool try_to_insert(int friend1, int friend2){
        bool status = false;
        auto num_pages = buffer_manager.getNumPages();
        for (size_t page_itr = 0; page_itr < num_pages; page_itr++) {

            auto newTuple = std::make_unique<Tuple>();

            auto friend1_field = std::make_unique<Field>(friend1);
            auto friend2_field = std::make_unique<Field>(friend2);

            newTuple->addField(std::move(friend1_field));
            newTuple->addField(std::move(friend2_field));
            auto& page = buffer_manager.getPage(page_itr);
            status = page->addTuple(std::move(newTuple));
            if (status == true){
                //std::cout << "Inserted into page: " << page_itr << "\n";
                buffer_manager.flushPage(page_itr);
                break;
            }
        }

        return status;
    }

    // insert function
    void insert(int friend1, int friend2) {
        tuple_insertion_attempt_counter += 1;

        if(tuple_insertion_attempt_counter >= max_number_of_tuples){
            return;
        }

        bool status = try_to_insert(friend1, friend2);

        // Try again after extending the database file
        if(status == false){
            buffer_manager.extend();
            bool status2 = try_to_insert(friend1, friend2);
            assert(status2 == true);
        }

        //newTuple->print();

        // Skip deleting tuples only once every hundred tuples
        if (tuple_insertion_attempt_counter % 100 != 0){
            auto& page = buffer_manager.getPage(0);
            page->deleteTuple(0);
            buffer_manager.flushPage(0);
        }
    }

    void scanTableToBuildIndex(){

        std::cout << "Scanning table to build index \n";

        auto num_pages = buffer_manager.getNumPages();
        size_t tuple_count = 0;

        for (size_t page_itr = 0; page_itr < num_pages; page_itr++) {
            auto& page = buffer_manager.getPage(page_itr);
            char* page_buffer = page->page_data.get();
            Slot* slot_array = reinterpret_cast<Slot*>(page_buffer);
            for (size_t slot_itr = 0; slot_itr < MAX_SLOTS; slot_itr++) {
                if (slot_array[slot_itr].empty == false){
                    assert(slot_array[slot_itr].offset != INVALID_VALUE);
                    const char* tuple_data = page_buffer + slot_array[slot_itr].offset;
                    std::istringstream iss(tuple_data);
                    auto loadedTuple = Tuple::deserialize(iss);
                    int key = loadedTuple->fields[0]->asInt();
                    int value = loadedTuple->fields[1]->asInt();

                    // Build indexes
                    tuple_count++;
                    hash_index.insertOrUpdate({key, value}, {key, value});
                }
            }
        }

        std::cout << "Tuple count: " << tuple_count << "\n";
    }

    void executeComplexQueries() {
        auto start_time = std::chrono::high_resolution_clock::now();


        // Mutual Friends
        for (int i = 1; i <= 99; i++) {
            for(int j=1;j<=99;j++) {
                int user_id1 = i;
                int user_id2 = j;
                auto mutual_friends = findMutualFriends(user_id1, user_id2, hash_index);
                // std::cout << "First level Mutual friends between User " << user_id1 << " and User " << user_id2 << ": ";
                for (int uid : mutual_friends) {
                    std::cout << uid << " ";
                }
                std::cout << std::endl;
            }
        }

        for (int i = 1; i <= 99; i++) {
            for(int j=1;j<=99;j++) {
                int user_id1 = i;
                int user_id2 = j;
                auto mutual_friends_second = findSecondLevelMutualFriends(user_id1, user_id2, hash_index);
                // std::cout << "Second level Mutual friends between User " << user_id1 << " and User " << user_id2 << ": ";
                for (int uid : mutual_friends_second) {
                    std::cout << uid << " ";
                }
                std::cout << std::endl;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_graph = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();


        std::cout << "Execution Time: " << duration_graph << " ms" << std::endl;
    }




};

int main() {



    BuzzDB db;

     // Add friendships
    db.insert(1, 2);
    db.insert(1, 4);
    db.insert(1, 5);
    db.insert(4, 3);
    db.insert(5, 3);
    db.insert(3, 5);
    db.insert(3, 4);
    db.insert(5, 1);
    db.insert(4, 1);
    db.insert(2, 1);
    //
    //
    db.scanTableToBuildIndex();

    // db.hash_index.print();
    db.executeComplexQueries();

    BuzzDB db2;
    std::vector<std::pair<int, int>> edges = {
        {1, 2}, {2, 1},
        {1, 3}, {3, 1},
        {1, 4}, {4, 1},
        {1, 5}, {5, 1},
        {2, 6}, {6, 2},
        {2, 7}, {7, 2},
        {2, 8}, {8, 2},
        {3, 9}, {9, 3},
        {3, 10}, {10, 3},
        {4, 11}, {11, 4},
        {4, 12}, {12, 4},
        {5, 13}, {13, 5},
        {5, 14}, {14, 5},
        {6, 15}, {15, 6},
        {6, 16}, {16, 6},
        {7, 17}, {17, 7},
        {7, 18}, {18, 7},
        {8, 19}, {19, 8},
        {8, 20}, {20, 8},
        {9, 11}, {11, 9},
        {9, 12}, {12, 9},
        {10, 13}, {13, 10},
        {10, 14}, {14, 10},
        {11, 15}, {15, 11},
        {12, 16}, {16, 12},
        {12, 17}, {17, 12},
        {13, 18}, {18, 13},
        {14, 19}, {19, 14},
        {14, 20}, {20, 14},
        {15, 18}, {18, 15},
        {16, 19}, {19, 16},
        {17, 20}, {20, 17}
    };
    for(auto friend_pair : edges) {
        db2.insert(friend_pair.first, friend_pair.second);
    }

    db2.scanTableToBuildIndex();

    // db.hash_index.print();
    db2.executeComplexQueries();

    BuzzDB db3;

    std::string filename = "dense_graph.txt";
    std::ifstream file(filename);

    if (file.is_open()) {
        int u, v;
        while (file >> u >> v) {
            db3.insert(u, v);
        }
        file.close();
    } else {
        std::cerr << "Unable to open file " << filename << std::endl;
    }
    db3.scanTableToBuildIndex();

    // db.hash_index.print();
    db3.executeComplexQueries();

    return 0;
}
