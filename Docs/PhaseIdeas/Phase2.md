# Vagrant Story Enhancements

## Scope

Improve the native port's presentation, controls, reliability, performance, and ease of use while preserving the original game. Do not add or remove story, characters, enemies, items, rooms, puzzles, modes, or gameplay systems. Keep combat rules, timings, progression, item behavior, and inventory capacities unchanged unless fixing a confirmed original bug; any such fix should be documented and checked against the original.

## Enhancements

### Rendering, display, and performance

1. **60 FPS support.** Render smoothly at higher refresh rates while keeping simulation, animation, combat, and input timing faithful to the original.
2. **Correct widescreen support.** Use the extra horizontal space to show more of the existing scene rather than stretching textures or geometry; preserve intended camera framing and do not reveal hidden areas or encounters.
3. **Resolution scaling.** Support modern display resolutions without changing the original assets or distorting the game's proportions.
4. **Aspect-ratio options.** Keep a correctly proportioned 4:3 presentation alongside widescreen, with suitable borders or background fill rather than stretching.
5. **Faithful image filtering.** Offer rendering choices that preserve the original look and avoid unwanted blur, shimmer, or excessive sharpening.
6. **Stable frame pacing.** Avoid uneven frame delivery, judder, tearing, and stutter independently of the selected frame rate.
7. **Correct rendering artifacts.** Fix visual seams, texture warping, z-fighting, incorrect transparency, and other port-specific rendering defects without redesigning assets.
8. **Separate rendering from game timing.** Prevent refresh rate, window focus, or slow hardware from changing combat windows, movement, puzzles, or cutscene timing.
9. **Reduce loading stalls.** Optimize asset loading and room transitions without cutting, skipping, or shortening story scenes or gameplay animations.
10. **Scale performance to modest PCs.** Provide reliable performance on lower-end hardware through efficient rendering and clear, non-destructive graphics options.
11. **Resize and display cleanly.** Support windowed, borderless, and fullscreen play, with correct aspect ratio and UI layout when the window or display changes.
Extra. Look at adding options for backend renderer, such as Vulkan, DirectX, or OpenGL, to improve performance and compatibility across different hardware configurations. Look into raytraced lighting for enhanced visual fidelity without altering the original assets. Look into filters that enhance image quality, such as anti-aliasing, bloom, or color grading, while preserving the original artistic intent.

### Controls and input

12. **Complete controller remapping.** Let players remap the existing actions, keep required actions accessible, and flag conflicting bindings before they cause problems.
13. **Audit button combinations.** Clearly document every existing button combination and its context so players can discover the original controls without changing what those combinations do.
14. **Show the right button prompts.** Detect the active controller and display its correct button labels; update prompts promptly when switching between controller and keyboard.
15. **Improve analog-stick handling.** Make dead zones configurable and eliminate drift or inconsistent response without altering the intended movement range.
16. **Tune free-look controls.** Make existing camera rotation responsive and consistent, with optional sensitivity and inversion settings that do not affect Ashley's movement or combat rules.
17. **Support keyboard and mouse.** Map desktop inputs to the original actions, including menu navigation and free-look, without adding new in-game actions.
18. **Handle controller reconnection.** Recognize a disconnected or newly connected controller cleanly and retain the player's chosen bindings.
19. **Handle focus changes safely.** Prevent missed or stuck inputs when players alt-tab, resize the window, or change display modes.

### Combat and menus

20. **Make chain prompts easier to read.** Improve the visibility and consistency of existing chain timing cues, while preserving the original timing window and combat balance.
21. **Capture chain inputs reliably.** Prevent dropped or duplicated inputs; success and failure must still follow the original timing rules.
22. **Improve targeting-sphere precision.** Make the existing target cursor easier to see and control without changing targetable body parts, hit areas, or damage.
23. **Clarify existing combat meters.** Improve the readability of HP, MP, and Risk indicators without changing their values, behavior, or regeneration.
24. **Clarify limb-condition feedback.** Make Ashley's existing body-part condition display easier to distinguish and read without changing its effects.
25. **Make battle menus more responsive.** Reduce input lag and unnecessary waits when opening, closing, or navigating menus; do not remove required animations or confirmations.
26. **Speed up weapon changes.** Provide a convenient binding or shorter path to the existing weapon-selection menu, while keeping the same weapons, equipment rules, and selection behavior.
27. **Clarify ability assignments.** Make the existing Chain and Defense Ability assignments easier to inspect and edit, with clear button mapping and no changes to learned abilities.
28. **Improve magic targeting feedback.** Keep the existing spell and target selection behavior, but make the current selection and target sphere easier to follow.
29. **Fix confirmed combat defects.** Correct port-introduced or demonstrable bugs in hit detection, target selection, defense timing, or damage calculation; preserve intended original behavior.

### Inventory, equipment, and workshops

30. **Improve item-container access.** Reduce unnecessary steps when moving between Ashley's inventory and existing storage containers, without changing the original inventory limits.
31. **Keep inventory sorting predictable.** Make the existing sort and category navigation consistent, retain the player's position where practical, and avoid accidental discard or selection changes.
32. **Make item search easier.** Improve navigation of the existing item categories and search function without adding items, categories, or hidden-item discovery.
33. **Compare equipment more clearly.** Present the existing weapon, armor, material, and affinity information in a readable, consistent way when reviewing equipment.
34. **Clarify workshop navigation.** Reduce needless menu movement through existing workshop commands while preserving all assembly, repair, combination, gem, and disassembly rules.
35. **Explain existing equipment attributes.** Improve the wording and layout of existing type, material, range, and affinity descriptions; do not invent stats or reveal undiscovered combination outcomes.
36. **Protect deliberate item actions.** Make destructive choices such as discarding or confirming a workshop operation clear and deliberate, without changing the original outcomes.

### Exploration, saves, accessibility, and stability

37. **Improve map legibility.** Make the existing map and its current-room information easier to read; show only information the original game has already revealed.
38. **Make map controls consistent.** Clearly communicate and reliably handle the existing map-rotation controls across supported input devices.
39. **Fix traversal inconsistencies.** Correct unintended collision, crate interaction, jumping, or ledge issues while keeping each puzzle's solution and layout unchanged.
40. **Prevent camera obstruction.** Fix camera clipping or occlusion that makes existing traversal or combat hard to see, without exposing unseen areas or changing encounters.
41. **Make existing saves more reliable.** Protect saving and loading at the original save points and storage containers against interrupted writes and corrupted files; do not add new save locations or autosave behavior.
42. **Preserve save compatibility.** Keep existing supported game saves usable across port updates, and report invalid or incompatible save files clearly rather than silently replacing them.
43. **Improve interface readability.** Scale existing menus and text cleanly across resolutions and keep content inside safe areas, without rewriting or removing the original script.
44. **Make existing visual cues accessible.** Improve contrast and distinguishability of existing UI indicators without changing gameplay information or adding new mechanics.
45. **Keep audio synchronized.** Prevent sound effects, music, and scene audio from drifting or dropping out; preserve the original soundtrack, cues, and mix.
46. **Fix text display defects.** Correct clipping, garbled characters, and inconsistent line wrapping while preserving the original dialogue and its meaning.
47. **Report failures clearly.** Provide actionable errors for crashes, missing or unreadable files, and unsupported settings instead of silently failing or losing progress.
48. **Test behavior against the original.** Regression-test controls, timing, combat, equipment, maps, puzzles, saves, and scene transitions so technical improvements do not change the game's content or rules.

## Research references

- [Vagrant Story English instruction manual](https://archive.org/details/vagrant_story_english) — original controls, menus, map, save flow, inventory categories and limits, and workshop commands.
- [IGN, Vagrant Story review](https://www.ign.com/articles/2000/05/23/vagrant-story) — contemporary discussion of targeting, Risk, Chain and Defense Abilities, affinities, and weapon combination.
- [Vagrant Story overview and gameplay summary](https://en.wikipedia.org/wiki/Vagrant_Story) — development context and a cross-referenced overview of the game's established systems and setting.
