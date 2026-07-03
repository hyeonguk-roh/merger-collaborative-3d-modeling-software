# Merger: Real-Time Collaborative 3D Modeler
*(Note: Source code is maintained in a private repository to comply with academic integrity policies for my university Capstone Project. This document serves as a technical architectural overview.)*

## Project Overview
**Merger** is a real-time collaborative 3D modeling software engineered from the ground up using **C++**. 

The application allows multiple users to connect simultaneously, load 3D meshes, and view/manipulate graphical assets in a shared virtual workspace with near-zero latency. This project required integrating low-level graphics rendering with highly concurrent, thread-safe network programming.

## Core Architecture & Features

### 1. Core Graphics & Mesh Pipeline (`render.cpp`, `mesh_loader.cpp`)
* **The Challenge:** Loading 3D models dynamically and rendering them efficiently while maintaining a high frame rate.
* **The Solution:** Engineered a custom rendering engine using **OpenGL** (via GLAD). Implemented a robust mesh loader to parse 3D object data and built optimized vertex/fragment shaders to handle real-time mesh transformations and camera manipulation.

### 2. High-Performance UI & App Loop (`merger_gui.cpp`, `merger_app.cpp`)
* **Features:** Built a responsive, developer-friendly interface using **Dear ImGui**. 
* **Logic:** The UI runs alongside the graphics loop, allowing users to select tools, manipulate object properties (scale, translation, rotation), and monitor network connections without dropping rendering frames.

### 3. Scene State Serialization (`scene_serialization.cpp`, `scene.cpp`)
* **The Challenge:** Synchronizing complex 3D scene data (matrices, vertex coordinates, object IDs) across a network efficiently.
* **The Solution:** Designed a custom serialization engine that converts in-memory `Scene` objects into lightweight binary payloads. This ensures network packets remain small, minimizing latency during real-time collaboration.

### 4. Concurrent Networking & Relaying (`signaling_server.cpp`, `client.cpp`, `lock_queue.h`)
* **The Challenge:** Handling asynchronous network traffic from multiple clients without causing race conditions in the main rendering thread.
* **The Solution:** Implemented a non-blocking network layer utilizing a custom **Signaling Server** architecture. Engineered a thread-safe `lock_queue` to safely pass network events (like remote object rotations) into the main application thread for rendering, eliminating desynchronization.

## Technical Stack
* **Language:** C++ 
* **Graphics API:** OpenGL, GLAD
* **GUI Framework:** Dear ImGui
* **Networking/Concurrency:** Asynchronous sockets, custom thread-safe lock queues
* **Architecture Design:** Client-Server model, Scene Serialization, Real-Time Event Polling
