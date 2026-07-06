# utah
*utah* is a renderer project written in modern C++ and Vulkan 1.3. 

## Progress Showcase
### Physically Based Rendering, with Image Based Lighting
![HelmetShowcase](https://github.com/user-attachments/assets/b6bff67b-643c-46bf-9c5e-c21b760763e8)

*All results measured on Windows 11 x64 - MSVC 2022 - Core Ultra 7 270K Plus - RTX 5080*
## Features Implemented
### Lighting & Shading
- Physically Based Rendering, with Image Based Lighting
- Shadow Maps for Directional/Spot/Point Lights, with Percentage-Closer Filtering
- Tone Mapping (PBR Neutral)
- HDR Environment Skybox
### Pipeline & Resource
- Bindless Textures via Descriptor Indexing
- Reverse-Z Depth Buffering
### Tooling & Debug
- Dear ImGUI Integration
- HLSL Compilation through DXC and Shader Hot Reload
## Libraries Used
//TODO
## Assets Used
//TODO
## References & Resources
- LearnOpenGL by Joey De Vries (https://learnopengl.com/)
- Vulkan Tutorial by Alexander Overvoorde (https://vulkan-tutorial.com/)
- Official Vulkan Documentation (https://docs.vulkan.org/spec/latest/index.html)
- Vulkan C++ examples and demos by Sascha Willems (https://github.com/SaschaWillems/Vulkan)
- Vulkanised 2024: Common Mistakes When Learning Vulkan by Charles Giessen (https://www.youtube.com/watch?v=0OqJtPnkfC8)
- Vulkanised 2025: So You Want to Write a Vulkan Renderer in 2025 by Charles Giessen (https://www.youtube.com/watch?v=7CtjMfDdTdg)
- Vulkanised 2026: Vulkan Now and Then (for Hobbyists) by Sascha Willems (https://www.youtube.com/watch?v=EshkHyYxb3A)
## Trivia
I've apparently commited to naming my projects after places, and for a renderer, no other name than "utah" was ever in the running.
