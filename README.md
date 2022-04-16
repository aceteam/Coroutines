# ACE Team Coroutines Plugin for UE4

This plugin was developed for the purpose of having an easy way to write complex routines that handle control flow spread across multiple frames in a robust and interruptible way. It's useful for scheduling async operations, reactive AI (e.g. behavior trees), animated visualizations, and other operations which you might want to spread over several frames.

## Background
Our experience with **SkookumScript** during **The Eternal Cylinder** proved that it was very useful to have the expressive power to easily spread out control flow over several frames, but the dwindling support for the plugin after Epic's acquisition led it to be difficult to maintain.

Also the addition of live coding made it viable to have fairly quick iteration times from C++.

The combination of these two factors led us to seek an implementation that captured most of the capabilities of **SkookumScript** without requiring a separate scripting language, and a very complex hard to maintain codebase.

