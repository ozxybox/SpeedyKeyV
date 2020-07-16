# SpeedyKeyV
### A decently fast KeyValue parser built for usability


Example code:
```cpp

KeyValueRoot kv("My RadKv"); // If you don't want to pass your string in the constructor, or if you want error reporting, use kv.Parse(yourStringHere);

// Writing to the KeyValue
kv.AddNode("AwesomeNode")->Add("Taco", "Time!"); // Adds the Node "AwesomeNode" {} and gives it a child pair "Taco" "Time!"
kv.Add("CoolKey", "CoolValue"); // Adds the KeyValue pair "CoolKey" "CoolValue"

// Optimizing access speeds
kv.Solidify(); // Use this if you have a big file and need quicker access times. Warning: It will make the kv read-only and deletes slower!

// Reading from the KeyValue
printf(kv["AwesomeNode"]["Taco"].value.string); // Accesses the node AwesomeNode's child, Taco, and prints Taco's value, "Time!"
printf(kv[2].value.string); // Accesses the third pair, CoolKey, and prints its value, CoolValue

// Printing the KeyValue
char printBuffer[1024];
kv.ToString(printBuffer, 1024); // Prints 1024 characters of the KeyValue to the buffer for printing
printf(printBuffer);

```
