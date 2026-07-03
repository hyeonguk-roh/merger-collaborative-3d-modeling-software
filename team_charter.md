# Team Charter Template
Last updated: [ 2025-10-04 ]

## Team Name: 100 People

### Team Members:
- Danny Roh
- Ciaran Krabak
- Sameer Jaura
- Amartya Jain

## 1. Project Summary
There are various tools that currently exist to aid in 3D modeling and design. However, many of these tools lack the infrastructure for seamless collaboration. The lack of collaborative tooling these softwares provide quickly becomes cumbersome in a team setting where many people might be working on a single scene. With currently available tools, the closest we can get to collaboration on a single project is a scenario where a team member sends a file around to their teammates, with only one person having access to it at a time. This is incredibly tedious, especially in cases where many quick edits must be made by different people. An example would be where each member of a team works on a different part of the environment, with each member needing to make small edits to their respective meshes within the single scene. A sequential workflow is inefficient and ends up costing teams valuable time. Our solution to this problem is Merger, a 3D modeling software built from the ground up with networked collaboration in mind, providing an efficient parallel workflow for teams.

Merger will have built in networking capabilities, providing official support for teams to seamlessly work on the same scenes together. Off-the-shelf networking solves the collaboration issues that other modelling softwares currently fail to address and allows anyone to immediately start working with others on their projects. Users will be able to make changes and see the changes made by others in real time. Merger will greatly increase efficiency in collaborative environments, where many people would have to make simultaneous changes to a scene. Time is one of the most important factors for many creative industries and Merger helps to solve this problem by getting rid of collaborative bottlenecks inherent in current 3D software tools.

Merger as a software provides useful solutions to various different industries as well as universities. Game companies can use Merger to cut down on game scene development time and quickly iterate on different prototypes. Oftentimes, game environments and scenes go through many different stages over the course of many months. Merger helps to reduce this time by cutting out the unnecessary burden that comes with collaboration via current 3D software. Likewise, Merger is useful for CAD (Computer Aided Design) purposes at engineering firms for similar purposes. Oftentimes, engineers do not work alone and need a way to easily share and work on designs with each other. Merger can also be utilized in an educational setting, allowing instructors to collaborate with their students as well as organize group projects in which students learn to collaborate with each other. This is invaluable in preparing students for the professional world, where working together is incredibly important.

Industry need for collaboration is an inherent part of many workflows. As addressed earlier, many current softwares do nothing to solve this bottleneck in collaboration. Merger provides this solution wrapped into a singular standalone app, making it a valuable part of every team’s toolkit.


## 2. Project Objectives
Our senior design project is a 3D collaborative modeling software. The collaborative part of the software includes built in networking that allows users to edit the same 3D scene in real time. Users are able to create models and manipulate vertices with each other, working together to create 3D scenes. The overall purpose of this project is meant to address the lack of networking and collaborative tools in current 3D modeling software.
	
This project has various technical aspects related to both graphics and networking. On the graphics side, technical challenges include drawing objects in 3D space, moving cameras, and having basic lighting. Since we are making a 3D modeler, various functionality such as adding and removing vertices, subdividing faces, and manipulating transforms all add technical complexity. On the networking side, we will use a centralized client-server architecture so users can work together on scenes. The server will handle synchronization issues, sharing scenes with new users, broadcasting changes of scenes to all users, and data races. Users can connect to the server and edit scenes, where their changes are broadcast across the network.
	
Along with technical complexity, there are many inherent algorithmic complexities in a product like this. For networking, synchronization and keeping a consistent state for all users is not-trivial. Likewise, preventing data races is difficult and requires careful protocols for handling concurrent access. Streaming models over the internet requires special care as well because 3D models can sometimes be very large. For the modeling side, things such as subdividing meshes and ensuring meshes aren’t corrupted when removing and adding vertices is also algorithmically complex.


### 2.1 Specific Goals
UI: Client has a graphical interface that displays the 3D scene with various other menus that show tools and a scene graph to the user.
- Main 3D canvas: The area marked by grids where the user can manipulate meshes as well as view the scene.
- Toolbar: A visual menu where users can select various tools like Select or Add Mesh in order to interact with the canvas.
- Scene Hierarchy: Another menu where users can select specific meshes within a scene and add hierarchy for combined meshes

Network Server: A central server architecture that allows for clients changes to the model to be broadcast across a network to all other clients in the session.
- Streaming model and scene data over internet
- Broadcasts changes made by client to all other clients
- Prevents things such as data races by ensuring one client can modify a given vertex at any time

Network Client: A client that can manipulate and modify meshes via their vertices to create complex 3D models. Changes are conveyed to a server.
- The application: standalone clientside application
- Send data to the server
- Receive data from the server
- Manipulate vertices on the screen

Modeling Component: Handles various algorithms and protocols for mesh manipulation in the 3D modeler
- Add / remove vertices
- Manipulate transforms
- Subdivide meshes

Graphics: 3D renderer that displays complex meshes and provides functionality such as cameras and basic lighting. 
- Given a group of vertices, can render the model to the screen
- Provides basic lighting to models
- Camera can move around scene freely to view models


## 3. Roles and Responsibilities

### 3.1 Team Lead
We won’t have a team lead for the project. The team will meet to create the board for each sprint and delegate tasks accordingly. Members can add to the sprint board as necessary throughout the sprint with team approval. We will rotate which members are responsible for the tasks below each week:
- reminding team members to post weekly status updates
- take notes during meetings and share in Slack channel
- schedule time for team to sync

### 3.2 Team Roles
| Member Name    | Strengths                        | Technical Responsibilities                            |
| -------------- | -------------------------------- | ----------------------------------------------------- |
| Danny Roh      | opengl, asio                     | Mainly networking and assisting graphics              |
| Ciaran Krabak  | opengl, c++, graphics algorithms | Mainly graphics and network integration with frontend |
| Sameer Jaura   | c++, asio                        | Mainly Graphics and assisting Networking              |
| Amartya Jain   | c++, asio                        | Mainly networking and graphics integration            |

### 3.3 Logistics
Roles will rotate on a weekly basis. For example, Amartya will do the weekly status update the first week. The next week he will be responsible for taking notes during the instructor meeting.
Rotation starts week of 9/29
- Amartya Jain : responsible for creating weekly status update tickets and moving them to done as the sprint progresses
- Sameer Jaura : responsible for taking notes during instructor meeting and posting them to slack
- Ciaran Krabak : responsible for booking first meeting time for gelman meeting
- Danny Roh : responsible for booking the second meeting time for gelman meeting

## 4. Communication Guidelines

### 4.1 Communication Tools
- We will use Slack for async communication between team members, mentors, and instructors
- We will use Zoom for weekly meetings with instructors and mentors
- We will use GitHub Projects to track sprint progress and post weekly updates


### 4.2 Meetings
- Weekly meetings with the team mentor will be held Thursdays at 4pm over Slack
- Weekly meetings with the instructor will be held Tuesdays at 5pm over Zoom
- Weekly meetings with team members will be held Thursdays at 2:30pm in person 

### 4.3 Documentation & Reporting

- Meeting notes will be documented and shared over Slack within 24 hours of each meeting.
- Team members will post weekly status updates to GitHub Projects no later than Sunday night outlining:
	- what they worked on over the past week
	- if they are blocked on anything
	- what they are currently working on
- All members will be given equal opportunity to participate in discussion and be open to listening to diverse perspectives on all topics/ideas.
- All members must be transparent about their progress, challenges, and any delays they anticipate.

## 5. Decision-Making Process
- **Consensus-Based Decisions**: Major decisions will be made through consensus among all team members. In case of a deadlock, a majority vote will have the final say. If no majority is reached, the team will reach out to their mentor and instructor for guidance.
- **Documentation**: All decisions must be documented and shared with the team to ensure clarity and alignment.
- **Conflict Resolution**: Any conflicts between team members should first be addressed internally through open communication. If unresolved, members must notify the instructor who will then mediate.

## 6. Performance Standards

### 6.1 Work Distribution
- **Flexibility**: If a member is unable to complete a task, they must notify the team as soon as possible. If a member is unable to complete a task and delays ensue, the team will organize a meeting to discuss reallocation or assistance with the task. Likewise, if a member feels as though they are unable to complete a task, they should reach out to the team as soon as possible for help.

### 6.2 Respect and Professionalism
- **Mutual Respect**: All members are expected to treat each other with respect and professionalism.
- **Constructive Feedback**: Feedback should be given and received constructively, focusing on improving the project.

## 7. Resource Allocation
- **Time Commitment**: All members are expected to spend ~15 hours of time outside of class per week on the project.
- **Workload Distribution**: Tasks will be assigned based on each member's expertise and availability. Each member is expected to complete their assigned tasks within the agreed-upon deadlines. 
- **Shared Resources:**: No shared resources

## 8. AI Use
AI tools such as Gemini and ChatGPT can be used for conceptual help or basic debugging purposes but code written exclusively by AI will not be allowed and PRs with AI code will be denied.

## 9. Signatures
By signing below, each team member agrees to uphold this team charter (add names as separate commits):
- Danny Roh , [2025-10-04]
- Ciaran Krabak , [2025-10-04]
- Sameer Jaura , [2025-10-04]
- Amartya Jain , [2025-10-04]

## 10. Charter Review and Updates
This charter was last updated on [2025-10-04].

It was previously modified on:
- [2025-10-01]
- [2025-10-02]
- [2025-10-03]
- [2025-10-04]