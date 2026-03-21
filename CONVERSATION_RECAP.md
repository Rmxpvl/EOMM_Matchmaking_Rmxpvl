# EOMM Implementation Discussion Recap

**Date**: 2026-03-21  
**Contributors**: Rmxpvl  

## Summary of Discussions

Over the course of our discussions regarding the implementation of the EOMM (Enhanced Online Matchmaking) system, we reached several key decisions and outlined our system design and proof objectives. Below is a comprehensive summary:

### 1. Decisions Made
- **Algorithm Selection**: We decided to utilize a hybrid matchmaking algorithm combining both skill-based and latency-based systems to ensure fair gameplay.
- **Data Management**: We will implement a centralized database structure to store player profiles and matchmaking stats, ensuring that the data is accessible in real-time.
- **User Interface**: A clean and intuitive UI design will be prioritized to enhance user experience during matchmaking.

### 2. System Design
- **Architecture**: The system will be built using microservices architecture, allowing us to independently scale different components such as matchmaking, user profiles, and lobby management.
- **Technology Stack**:
  - Backend: Node.js with Express
  - Database: MongoDB
  - Frontend: React.js

### 3. Proof Objectives
- **Performance Testing**: We aim to conduct thorough performance testing to ensure the system can handle up to 1000 concurrent users without degrading the experience.
- **User Acceptance Testing**: Engaging a small group of users for acceptance testing before full rollout to gather feedback and make necessary adjustments.

### Conclusion
This document is a living record of the decisions and designs agreed upon during our discussions. Further updates will be made as development progresses.