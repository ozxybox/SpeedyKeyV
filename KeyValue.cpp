//
// SpeedyKeyV
// https://github.com/ozxybox/SpeedyKeyV
//

#include "KeyValue.h"
#include <cstring>
#include <cstdlib>

// For min and max
#include <algorithm>

#define ALLOW_QUOTELESS_STRINGS 1

#define BLOCK_BEGIN '{'
#define BLOCK_END '}'
#define STRING_CONTAINER '"'

#define SINGLE_LINE_COMMENT "//"

#define TAB_STYLE "    "

#define POOL_STARTING_LENGTH 0
#define POOL_INCREMENT_LENGTH 4

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

//////////////////////
// Helper Functions //
//////////////////////
// Notes:
// These pass const char* references so that they can increment them. Otherwise, the parse loop would get stuck!

// Will return true on end of string!
inline bool IsWhitespace(char c)
{
	// Anything that isn't ASCII is treated as not whitespace
	return (c < '!' || c > '~') && ( c >= 0 && c <= 127 );
}

// Is c a character used by something else?
inline bool IsSpecialCharacter(char c, char cc)
{
	return c == STRING_CONTAINER || c == BLOCK_BEGIN || c == BLOCK_END || 
		// If it's a line comment then it should always have another / right after the first one
		(c == SINGLE_LINE_COMMENT[0] && cc == SINGLE_LINE_COMMENT[1]);
}

// Ends on the first character after whitespace
void SkipWhitespace(const char*& str)
{
startOfSkip:
	for (; IsWhitespace(*str) && *str; str++);

	// Not really whitespace, but we might as well check if there's any comments here...
	if (strncmp(str, SINGLE_LINE_COMMENT, sizeof(SINGLE_LINE_COMMENT) - 1) == 0)
	{
		// Read until end line
		for (char c = *str; c && c != '\r' && c != '\n'; c = *++str);

		// There might be more whitespace after the comment, so we should go back and check for more
		goto startOfSkip;
	}
}

KeyValueErrorCode ReadQuotedString(const char*& str, kvString_t& inset)
{
	/*
	We probably should check if this is a quote, but, we already know that it's a quote due to earlier logic.
	If this is a problem ever, just uncomment this
	if(*str != STRING_CONTAINER)
		return KeyValueErrorCode::QUOTED_STRING_MUST_BEGIN_WITH_QUOTE;
	*/
	str++;

	inset.string = const_cast<char*>(str);

	for (char c = *str; c && c != STRING_CONTAINER; c = *++str);

	if (!*str)
	{
		// We hit the end of the file, but the string was never closed
		return KeyValueErrorCode::INCOMPLETE_STRING;
	}

	inset.length = str - inset.string;

	// Skip over the quote 
	str++;

	return KeyValueErrorCode::NONE;
}

#if ALLOW_QUOTELESS_STRINGS

kvString_t ReadQuotelessString(const char*& str)
{
	kvString_t inset;
	inset.string = const_cast<char*>(str);

	// Read until whitespace, special character, or end of string 
	for (; !IsWhitespace(*str) && !IsSpecialCharacter(str[0], str[1]); str++ );

	inset.length = str - inset.string;
	return inset;
}

#endif // ALLOW_QUOTELESS_STRINGS


////////////////////
// Key Value Pool //
////////////////////


template<typename T>
KeyValuePool<T>::KeyValuePool()
{

	position = 0;

	firstPool = new PoolChunk(POOL_STARTING_LENGTH);

	currentPool = firstPool;
}

template<typename T>
KeyValuePool<T>::~KeyValuePool()
{
	if (firstPool)
		Drain();
}

template<typename T>
void KeyValuePool<T>::Drain()
{
	PoolChunk* nextPool = firstPool;
	PoolChunk* old;

	while (nextPool)
	{
		free(nextPool->pool);
		old = nextPool;
		nextPool = nextPool->next;
		delete old;
	}

	firstPool = nullptr;
	currentPool = nullptr;
}

template<typename T>
T* KeyValuePool<T>::Create()
{
	if (!IsFull())
	{
	returnKV:
		T* kv = &currentPool->pool[position];
		position++;
		return kv;
	}

	// If the pool is full, we have to allocated a new one, and try again
	position = 0;
	size_t newLength = currentPool->length + POOL_INCREMENT_LENGTH;

	currentPool = new PoolChunk(newLength, currentPool);

	goto returnKV;
}

template<typename T>
KeyValuePool<T>::PoolChunk::PoolChunk(size_t length)
{
	next = nullptr;

	pool = (T*)malloc(sizeof(T) * length);
	this->length = length;
}

template<typename T>
KeyValuePool<T>::PoolChunk::PoolChunk(size_t length, PoolChunk* last) : PoolChunk(length)
{
	last->next = this;
}


////////////////////
// Key Value Root //
////////////////////

KeyValueRoot::KeyValueRoot(const char* str) : KeyValueRoot()
{
	Parse(str);
}

KeyValueRoot::KeyValueRoot()
{
	bufferSize = 0;
	solidified = false;
	rootNode = this;

	key = { nullptr, 0 };

	isNode = true;
	data.node = { nullptr, nullptr, 0 };

	stringBuffer = nullptr;

}

KeyValueRoot::~KeyValueRoot()
{
	if (bufferSize > 0)
		free(stringBuffer);


	// writePoolStrings are allocated on creation of a node.. We have to clean all of these up manually :(

	for (KeyValuePool<char*>::PoolChunk* current = writePoolStrings.firstPool; current != writePoolStrings.currentPool; current = current->next)
	{
		for (size_t i = 0; i < current->length; i++)
		{
			delete[] current->pool[i];
		}
	}

	// The last pool might not be totally filled out...
	for (size_t i = 0; i < writePoolStrings.position; i++)
	{
		delete[] writePoolStrings.currentPool->pool[i];
	}

}

void KeyValueRoot::Solidify()
{
	if (solidified)
		return;
	solidified = true;

	// We need to take the pool, move the stuff into their correct positions, and delete it
	if (data.node.childCount > 0)
	{
		KeyValue::Solidify();
	}

	// Copied of all of these values will be made. No need to retain the pools...
	readPool.Drain();
	writePool.Drain();
	// Sadly, we can't drain the string pool... It contains keys and values for newly added nodes and pairs
}

KeyValueErrorCode KeyValueRoot::Parse(const char* str)
{
	if ( !str )
		return KeyValueErrorCode::NO_INPUT;

	KeyValueErrorCode err = KeyValue::Parse(str, true);
	if (err != KeyValueErrorCode::NONE)
		return err;

	if (bufferSize > 0)
	{
		stringBuffer = (char*)malloc(sizeof(char) * bufferSize);

		// Can't straight pass it, otherwise it'd mess with it
		char* temp = stringBuffer;
		BuildData(temp);
	}

	// All good. Return no error
	return KeyValueErrorCode::NONE;
}


///////////////
// Key Value //
///////////////

// An invalid KV for use in returns with references
KeyValue& KeyValue::GetInvalid()
{
	static const KeyValue invalid(true);

	return const_cast<KeyValue&>(invalid);
}


void KeyValue::Solidify()
{
	KeyValue* newArray = new KeyValue[data.node.childCount];

	KeyValue* current = data.node.children;

	data.node.children = newArray;

	// Is it worth block copying out of the pool?

	size_t cc = data.node.childCount;
	for (size_t i = 0; i < cc; i++)
	{
		memcpy(&newArray[i], current, sizeof(KeyValue));

		if (newArray[i].isNode && newArray[i].data.node.childCount > 0)
			newArray[i].Solidify();

		// Maintains compatibility with linked list code
		newArray[i].next = &newArray[i + 1];

		current = current->next;
	}

	newArray[cc - 1].next = nullptr;

}


KeyValue& KeyValue::InternalGet(const char* keyName) const
{
	if (!isNode || data.node.childCount <= 0 || !IsValid())
		return GetInvalid();


	// If we're solid, we can use a quicker route
	if (rootNode->solidified)
	{
		size_t cc = data.node.childCount;
		for (size_t i = 0; i < cc; i++)
		{
			if (strcasecmp(data.node.children[i].key.string, keyName) == 0)
			{
				return data.node.children[i];
			}
		}
	}
	else
	{
		for (KeyValue* current = data.node.children; current; current = current->next)
		{
			if (strcasecmp(current->key.string, keyName) == 0)
			{
				return *current;
			}
		}
	}

	return GetInvalid();
}

KeyValue& KeyValue::InternalAt(size_t index) const
{
	// If we cant get something, return invalid
	if(!isNode || data.node.childCount <= 0 || index < 0 || index >= data.node.childCount || !IsValid())
		return GetInvalid();

	if (rootNode->solidified)
	{
		return data.node.children[index];
	}
	else
	{
		KeyValue* current = data.node.children;
		for (int i = 0; i != index; i++)
		{
			current = current->next;
		}
		return *current;
	}
}


KeyValue* KeyValue::Add(const char* keyName, const char* value)
{
	// Can't add to a solid kv without kids!
	if (rootNode->solidified && !isNode)
		return nullptr;


	size_t keyLength = strlen(keyName);
	char*& copiedKey = *rootNode->writePoolStrings.Create();
	copiedKey = new char[keyLength + 1];
	memcpy(copiedKey, keyName, keyLength);
	copiedKey[keyLength] = '\0';

	size_t valueLength = strlen(value);
	char*& copiedValue = *rootNode->writePoolStrings.Create();
	copiedValue = new char[valueLength + 1];
	memcpy(copiedValue, value, valueLength);
	copiedValue[valueLength] = '\0';

	KeyValue* newKV = CreateKVPair({ copiedKey, keyLength }, { copiedValue, valueLength }, rootNode->writePool);
	newKV->next = nullptr;

	if (data.node.children)
	{
		data.node.lastChild->next = newKV;
	}
	else
	{
		data.node.children = newKV;
	}
	data.node.lastChild = newKV;
	data.node.childCount++;

	return newKV;
}

KeyValue* KeyValue::AddNode(const char* keyName)
{
	// Can't add to a solid kv without kids!
	if (rootNode->solidified && !isNode)
		return nullptr;

	KeyValue* node = rootNode->writePool.Create();

	size_t keyLength = strlen(keyName);
	char*& copiedKey = *rootNode->writePoolStrings.Create();
	copiedKey = new char[keyLength + 1];
	memcpy(copiedKey, keyName, keyLength);
	copiedKey[keyLength] = '\0';

	node->key = { copiedKey, keyLength };

	node->isNode = true;
	node->data.node = { nullptr, nullptr, 0 };

	node->rootNode = rootNode;
	node->next = nullptr;

	if (data.node.childCount == 0)
	{
		data.node.children = node;
	}
	else
	{
		data.node.lastChild->next = node;
	}
	data.node.lastChild = node;
	data.node.childCount++;
	
	return node;
}

bool KeyValue::IsValid() const
{
	// The invalid KV is always invalid, and infinite loops are invalid 
	return this != &GetInvalid()
		&& next != this && data.node.children != this && data.node.lastChild != this;
}

KeyValue::KeyValue(bool invalid) : data({})
{
	if (invalid)
	{
		// Zero the key and root
		key = { nullptr, 0 };
		rootNode = nullptr;
		
		// Explicitly invalid data
		next = this;
		isNode = true;
		data.node.children = this;
		data.node.lastChild = this;
		data.node.childCount = 0;
	}
}

KeyValue::~KeyValue()
{
	if (isNode && data.node.childCount > 0 && rootNode->solidified)
	{
		delete[] data.node.children;
	}
}

KeyValue* KeyValue::CreateKVPair(kvString_t keyName, kvString_t string, KeyValuePool<KeyValue>& pool)
{
	KeyValue* kv = pool.Create();
	kv->key = keyName;
	kv->data.leaf.value = string;
	kv->isNode = false;
	kv->rootNode = rootNode;
	return kv;
}

KeyValueErrorCode KeyValue::Parse(const char*& str, const bool isRoot)
{
	KeyValue* lastKV = nullptr;
	char c;
	for (;;)
	{
		SkipWhitespace(str);

		c = *str;

		kvString_t pairkey;

		// Parse the key out
		switch (c)
		{

		case STRING_CONTAINER:
		{
			KeyValueErrorCode error = ReadQuotedString(str, pairkey);

			if (error != KeyValueErrorCode::NONE)
				return error;

			break;
		}
		case BLOCK_END:

			// Make sure we skip a char here so that the above layer doesn't read it!
			str++;

			// If we hit a block end as root, we've got a syntax error on our hands... Let's skedaddle!
			if (isRoot)
				return KeyValueErrorCode::UNEXPECTED_END_OF_BLOCK;

			// Otherwise, we should be totally fine to just return at this point
			goto end;


			// Did we hit the end of the file?
		case 0:
			// If we're root and at the end of the file after this whitespace skip, that means there's no next kv pair. End now
			if (isRoot)
				goto end;

			// If we're not root and at the end of the file after this whitespace skip, we've failed to find the block end
			return KeyValueErrorCode::INCOMPLETE_BLOCK;

		case BLOCK_BEGIN:
			return KeyValueErrorCode::UNEXPECTED_START_OF_BLOCK;

#if ALLOW_QUOTELESS_STRINGS
			// It's gotta be a quoteless string
		default:

			pairkey = ReadQuotelessString(str);

			break;
#endif
		}

		rootNode->bufferSize += pairkey.length + 1; // + 1 for \0

		// We've got our key, so let's find its value

		SkipWhitespace(str);

		c = *str;


		KeyValue* pair;


		// Same kinda stuff as earlier but a bit different for the value
		switch (c)
		{

		case STRING_CONTAINER:
		{
			kvString_t stringValue;
			KeyValueErrorCode error = ReadQuotedString(str, stringValue);

			if (error != KeyValueErrorCode::NONE)
				return error;

			pair = CreateKVPair(pairkey, stringValue, rootNode->readPool);

			rootNode->bufferSize += stringValue.length + 1; // + 1 for \0
			break;
		}
		case BLOCK_BEGIN:
		{
			pair = rootNode->readPool.Create();
			pair->rootNode = rootNode;

			//skip over the BLOCK_BEGIN
			str++;
			pair->isNode = true;
			pair->data.node = { nullptr, nullptr, 0 };
			KeyValueErrorCode error = pair->Parse(str, false);
			if (error != KeyValueErrorCode::NONE)
				return error;

			pair->key = pairkey;

			break;
		}
		case 0:
			// Hit EOF before the value
			return KeyValueErrorCode::INCOMPLETE_PAIR;

		case BLOCK_END:
			return KeyValueErrorCode::UNEXPECTED_END_OF_BLOCK;

#if ALLOW_QUOTELESS_STRINGS
			// It's gotta be a quoteless string
		default:
		{

			kvString_t stringValue = ReadQuotelessString(str);
			pair = CreateKVPair(pairkey, stringValue, rootNode->readPool);

			rootNode->bufferSize += stringValue.length + 1; // + 1 for \0

			break;
		}
#endif
		}


		data.node.childCount++;
		if (lastKV)
		{
			lastKV->next = pair;
		}
		else
		{
			data.node.children = pair;
		}
		lastKV = pair;

	}
end:
	if (lastKV)
	{
		lastKV->next = nullptr;
		data.node.lastChild = lastKV;
	}

	return KeyValueErrorCode::NONE;
}

// Copies all of the keys and values out of the input string and copies them all into a massive buffer.
void KeyValue::BuildData(char*& destBuffer)
{
	KeyValue* current = data.node.children;
	for (size_t i = 0; i < data.node.childCount; i++)
	{
		// Copy the string in, null terminate it, and increment the destBuffer
		memcpy(destBuffer, current->key.string, current->key.length);
		destBuffer[current->key.length] = '\0';
		current->key.string = destBuffer;
		destBuffer += current->key.length + 1;

		if (current->isNode)
		{
			if (current->data.node.childCount > 0)
			{
				current->BuildData(destBuffer);
			}
		}
		else
		{
			memcpy(destBuffer, current->data.leaf.value.string, current->data.leaf.value.length);
			destBuffer[current->data.leaf.value.length] = '\0';
			current->data.leaf.value.string = destBuffer;
			destBuffer += current->data.leaf.value.length + 1;
		}

		current = current->next;
	}

}

// Copies memory and shifts the inputs
void CopyAndShift(char*& dest, char* src, size_t& destLength, size_t srcLength)
{
	size_t minLength = std::min(destLength, srcLength);
	memcpy(dest, src, minLength);
	destLength -= minLength;
	dest += minLength;
}

void CopyAndShift(char*& dest, const char* src, size_t& destLength, size_t srcLength)
{
	return CopyAndShift(dest, const_cast<char*>(src), destLength, srcLength);
}


void TabFill(char*& str, size_t& maxLength, int tabCount)
{
	for (int i = 0; i < tabCount; i++)
	{
		CopyAndShift(str, TAB_STYLE, maxLength, sizeof(TAB_STYLE) - 1); // - 1 due to null terminator
	}
}

void KeyValue::ToString(char*& str, size_t& maxLength, int tabCount) const
{
	// Make a solidified version?

	for (KeyValue* current = data.node.children; current; current = current->next)
	{

		TabFill(str, maxLength, tabCount);

		// Copy in the key
		CopyAndShift(str, "\"", maxLength, 1);
		CopyAndShift(str, current->key.string, maxLength, current->key.length);
		CopyAndShift(str, "\" ", maxLength, 2);

		if (current->isNode)
		{
			
			CopyAndShift(str, "\n", maxLength, 1);
			TabFill(str, maxLength, tabCount);
			CopyAndShift(str, "{\n", maxLength, 2);

			current->ToString(str, maxLength, tabCount + 1);

			TabFill(str, maxLength, tabCount);
			CopyAndShift(str, "}\n", maxLength, 2);
		}
		else
		{
			
			// Copy in the value
			CopyAndShift(str, "\"", maxLength, 1);
			CopyAndShift(str, current->data.leaf.value.string, maxLength, current->data.leaf.value.length);
			CopyAndShift(str, "\"\n", maxLength, 2);

		}
	}
}

char* KeyValue::ToString() const
{
	// Length of all kvs + 1 for the null terminator
	size_t length = ToStringLength(0) + 1;

	char* str = new char[length];
	
	ToString(str, length);

	// Null terminate it
	str[length - 1] = 0;
	
	return str;
}

size_t KeyValue::ToStringLength(int tabCount) const
{
	size_t len = 0;
	for (KeyValue* current = data.node.children; current; current = current->next)
	{
		len += tabCount * sizeof(TAB_STYLE);

		// String container + Key + String container 
		len += sizeof(STRING_CONTAINER) + current->key.length + sizeof(STRING_CONTAINER);

		if (current->isNode)
		{
			// If we have kids, new line + tabs + start block + new line
			len += 1 + tabCount * sizeof(TAB_STYLE) + sizeof(BLOCK_BEGIN) + 1;

			len += current->ToStringLength(tabCount + 1);

			// End line + tabs + end block + end line
			len += 1 + tabCount * sizeof(TAB_STYLE) + sizeof(BLOCK_END) + 1;
		}
		else
		{
			// If we don't have any children, we just have a value

			// Space + string container + value + string container + new line
			len += 1 + sizeof(STRING_CONTAINER) + current->data.leaf.value.length + sizeof(STRING_CONTAINER) + 1;

		}
	}

	return len;
}
