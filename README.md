# Moderated_LMS_Software_Engineering

## 1. Project Overview

The **Moderated Learning Management System (LMS)** is a C++17-based application designed to support the management, submission, assessment and moderation of academic work.

The system provides a structured environment in which students submit academic work, lecturers assess submissions, Heads of Department (HoDs) oversee moderation, and administrators manage the overall platform.

The project focuses on providing a reliable, secure and maintainable system that can operate in environments with limited computing resources and unreliable internet connectivity.

## 2. Project Objectives

The main objectives are to:

- Provide a centralised platform for managing academic submissions and assessment.
- Support different user roles with appropriate permissions.
- Allow lecturers to assess student submissions and record marks.
- Support moderation and review of academic results.
- Maintain reliable records using an SQLite database.
- Protect user accounts and sensitive information.
- Provide an efficient system suitable for older computers.
- Support automated testing and maintainable software development.

## 3. System Users and Roles

The system supports four main user roles:

| Role | Responsibilities |
|---|---|
| Administrator | Manages users, accounts and system-level operations. |
| Lecturer | Manages academic work, assesses submissions and records marks. |
| Head of Department (HoD) | Oversees academic moderation and reviews assessment results. |
| Student | Accesses academic activities and submits work for assessment. |

Access to system functionality is controlled according to the user's assigned role.

## 4. Main Features

- User authentication and account management.
- Role-based access control.
- Academic submission management.
- Assessment and mark recording.
- Moderation and review workflows.
- Persistent storage using SQLite.
- Secure password handling.
- Input validation and error handling.
- Automated unit and integration testing.
- Structured and maintainable C++ implementation.

## 5. Technologies Used

| Technology | Purpose |
|---|---|
| C++17 | Main programming language. |
| SQLite | Database and persistent storage. |
| CMake | Build configuration and project compilation. |
| libsodium | Password security and cryptographic operations. |
| Argon2id | Secure password hashing. |
| GoogleTest / Catch2 | Automated software testing. |
| Git and GitHub | Version control and collaboration. |

## 6. System Architecture

The application follows a modular design that separates major responsibilities, including:

- **Presentation layer:** Handles user interaction.
- **Business logic layer:** Implements system rules and workflows.
- **Data access layer:** Handles database operations.
- **Security layer:** Supports authentication, password protection and access control.
- **Testing layer:** Verifies the behaviour and reliability of system components.

This separation improves maintainability, testability and future development.

## 7. Installation and Setup

### Prerequisites

Ensure that the following tools are installed:

- A C++17-compatible compiler (GCC or Clang).
- CMake.
- SQLite development libraries.
- libsodium development libraries.
- Git.
- GoogleTest or Catch2, depending on the project's test configuration.

### Clone the Repository

```bash
git clone <REPOSITORY_URL>
cd <PROJECT_DIRECTORY>
```

### Configure the Project

```bash
cmake -S . -B build
```

### Compile the Application

```bash
cmake --build build
```

### Run the Tests

```bash
ctest --test-dir build --output-on-failure
```

**Note:** These commands assume that the root directory contains a `CMakeLists.txt` file and that the project has registered its tests with CTest. Follow the actual project configuration if the build structure differs.

## 8. Database

The system uses SQLite for persistent data storage.

The database is intended to maintain information required for system operations, including user accounts, academic submissions, assessment records and moderation-related information.

Database operations are handled through the application's data access components.

## 9. Security

Security is an important part of the system.

The project incorporates security measures such as:

- Password hashing using Argon2id through libsodium.
- Role-based access control.
- Input validation.
- Controlled access to system functionality.
- Database operation safeguards.
- Error handling to reduce unintended system behaviour.

Passwords should never be stored as plain text.

## 10. Testing and Quality Assurance

Automated testing is used to verify the correctness of the application's components and business logic.

Testing focuses on:

- Authentication and user management.
- Role permissions.
- Submission management.
- Assessment operations.
- Moderation workflows.
- Database operations.
- Input validation and error handling.

The project uses automated testing frameworks to support repeatable verification during development.

## 11. Project Scope

The project covers the design and implementation of a moderated LMS, including its main user roles, academic workflows, data persistence, security mechanisms and testing.

The system is developed with consideration for institutions that may use older computers and have limited or inconsistent internet connectivity.

## 12. Project Team

**Project:** Moderated Learning Management System  
**Course:** CS4433
**Institution:** National University of Lesotho  
**Academic Year:** 2026–2027

## 13. Repository Structure

The repository is organised to separate project documentation, implementation files, testing resources and supporting materials.

Refer to the repository folders for the actual source code, documentation, database resources and test files.

## 14. Future Improvements

Possible future improvements include:

- Enhanced reporting and analytics.
- Additional moderation and approval workflows.
- Improved user interface and accessibility.
- Expanded automated test coverage.
- Additional database backup and recovery features.
- Further performance optimisation.


