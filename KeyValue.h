//
// SpeedyKeyV
// https://github.com/ozxybox/SpeedyKeyV
//

#pragma once

///////////////
// Key Value //
///////////////
// Usage:
//
// KeyValueRoot kv("My RadKv"); // If you don't want to pass your string in the constructor, use kv.Parse(yourStringHere);
//
// // Writing to the KeyValue
// kv.AddNode("AwesomeNode")->Add("Taco", "Time!"); // Adds the Node "AwesomeNode" {} and gives it a child pair "Taco" "Time!"
// kv.Add("CoolKey", "CoolValue"); // Adds the KeyValue pair "CoolKey" "CoolValue"
//
// // Optimizing speeds
// kv.Solidify(); // Use this if you have a big file and need quicker access times. Warning: It will make the kv read-only!
//
// // Reading from the KeyValue
// printf(kv["AwesomeNode"]["Taco"].Value().string); // Accesses the node AwesomeNode's child, Taco, and prints Taco's value
// printf(kv[2].Value().string); // Accesses the third pair, CoolKey, and prints its value, CoolValue
//
// // Printing the KeyValue
// char printBuffer[1024];
// kv.ToString(printBuffer, 1024); // Prints 1024 characters of the KeyValue to the buffer for printing
// printf(printBuffer);
//

#include <cstddef>

enum class KeyValueErrorCode
{
	NONE,

	INCOMPLETE_BLOCK,
	INCOMPLETE_PAIR,
	UNEXPECTED_START_OF_BLOCK,
	UNEXPECTED_END_OF_BLOCK,
	INCOMPLETE_STRING,
	NO_INPUT,
};

// Little helper struct for keeping track of strings
struct kvString_t
{
	kvString_t() :
		string(nullptr), length(0) {}
	kvString_t(char* _string, size_t _length) :
		string(_string), length(_length) {}

	char* string;
	size_t length;
};

class KeyValueRoot;

template<typename T>
class KeyValuePool;

class KeyValue
{
public:

	KeyValue& Get(const char* keyName)				{ return InternalGet( keyName ); }
	const KeyValue& Get(const char* keyName) const	{ return (const KeyValue&)InternalGet( keyName ); }
	[[deprecated]] KeyValue& Get(size_t index)					{ return InternalAt( index ); }
	[[deprecated]] const KeyValue& Get(size_t index) const		{ return (const KeyValue&)InternalAt( index ); }
	KeyValue& At(size_t index)					{ return InternalAt( index ); }
	const KeyValue& At(size_t index) const		{ return (const KeyValue&)InternalAt( index ); }

	inline KeyValue& operator[](const char* keyName) { return Get(keyName); }
	inline const KeyValue& operator[](const char* keyName) const { return Get(keyName); }
	inline KeyValue& operator[](size_t index) { return At(index); }
	inline const KeyValue& operator[](size_t index) const { return At(index); }


	// These two only work for classes with children!
	KeyValue* Add(const char* key, const char* value);
	KeyValue* AddNode(const char* key);


	void ToString(char* str, size_t maxLength) const { ToString(str, maxLength, 0); if (maxLength > 0) str[0] = '\0'; }
	char* ToString() const;

	bool IsValid() const;


	const kvString_t& Key() const { return key; }

	bool HasChildren() const { return isNode; }
	size_t ChildCount() const { return isNode ? data.node.childCount : 0; }
	KeyValue* Children() const { return isNode ? data.node.children : nullptr; }

	kvString_t Value() const { return isNode ? kvString_t(nullptr, 0u) : data.leaf.value; }
	
	KeyValue* LastChild() { return data.node.lastChild; }
	const KeyValue* LastChild() const { return data.node.lastChild; }

	KeyValue* Next() { return next; }
	const KeyValue* Next() const { return next; }

protected:

	KeyValue() : data( {} ) {}

	// This is used for creating the invalid kv
	// Could be better?
	KeyValue(bool invalid);

	// Deleting should really only be done by the root
	~KeyValue();

	KeyValue& InternalGet(const char* keyName) const;
	KeyValue& InternalAt(size_t index) const;

	KeyValue* CreateKVPair(kvString_t keyName, kvString_t string, KeyValuePool<KeyValue>& pool);

	template<bool isRoot, bool useEscapeSequences>
	KeyValueErrorCode Parse(const char*& str);
	template<bool useEscapeSequences>
	void BuildData(char*& destBuffer);
	void Solidify();


	void ToString(char*& str, size_t& maxLength, int tabCount) const;
	size_t ToStringLength(int tabCount) const;

	// An invalid KV for use in returns with references
	static KeyValue& GetInvalid();

	// Top level root parent node
	KeyValueRoot* rootNode;

	// Next sibling pair
	KeyValue* next;


	kvString_t key;

	// These two structs must be the same size!!
	union
	{
		struct 
		{
			kvString_t value;
		} leaf;

		struct
		{
			KeyValue* children;
			KeyValue* lastChild;
			unsigned int  childCount;
		} node;
	
	} data;

	// Determines whether we should be using data.node or data.leaf
	bool isNode;

};

template<typename T>
class KeyValuePool
{
private:
	KeyValuePool();
	~KeyValuePool();

	void Drain();

	T* Create();

	inline bool IsFull() { return position >= currentPool->length; }

	class PoolChunk
	{
	public:
		PoolChunk(size_t length);
		PoolChunk(size_t length, PoolChunk* last);

		T* pool;
		size_t length;

		PoolChunk* next;
	};

	size_t position;

	PoolChunk* firstPool;
	PoolChunk* currentPool;

	// Nothing other than KeyValue and KeyValueRoot should touch this!
	friend KeyValue;
	friend KeyValueRoot;
};

class KeyValueRoot : public KeyValue
{
public:
	KeyValueRoot(const char* str);
	KeyValueRoot();
	~KeyValueRoot();

	// No copying! It'll be very expensive!
	KeyValueRoot( const KeyValueRoot& ) = delete;
	// No moving! It'll require us to update many references and be expensive!
	KeyValueRoot( KeyValueRoot&& ) = delete;


	// Makes access times faster and decreases memory usage at the cost of irreversibly making the kv read-only and slowing down delete time
	void Solidify();
	KeyValueErrorCode Parse(const char* str, bool useEscapeSequences = false);

private:

	// This string buffer exists to hold *all parsed* key and value strings. 
	char* stringBuffer;
	// bufferSize is tallied up during the parse as the total length of all parsed strings, and stringBuffer is allocated using it.
	size_t bufferSize;

	KeyValuePool<KeyValue> readPool;
	KeyValuePool<KeyValue> writePool;
	KeyValuePool<char*> writePoolStrings;

	bool solidified;

	friend KeyValue;
};
