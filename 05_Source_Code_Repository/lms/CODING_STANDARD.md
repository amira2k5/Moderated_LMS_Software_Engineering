# Coding standard

Based on the C++ Core Guidelines and the Google C++ Style Guide, with these project rules.

1. **Language and warnings.** C++17, no compiler extensions. The code builds with `-Wall -Wextra -Wpedantic` and no warnings.
2. **Layering.** UI calls services; services call repository interfaces; only `data.cpp` touches SQLite.
   No SQL outside `data.cpp` and `schema.cpp`. Domain code has no I/O.
3. **Naming.** Types `PascalCase`; functions and variables `camelCase`; data members end with `_`; constants `kPascalCase`;
   files `lower_snake_case`; SQL tables and columns `snake_case`.
4. **Formatting.** Two-space indent, no tabs, 120 columns at most, braces on the same line (see `.editorconfig`).
5. **Ownership.** No raw `new` or `delete`. Use `std::unique_ptr` for ownership and references for borrowed objects.
   Resources (statements, transactions) are released by destructors (RAII).
6. **Errors.** Throw a subclass of `LmsError` with a message a school user can understand. Never throw strings or ints.
   Only the console loop catches errors and prints them.
7. **Validation.** Every service method validates its input before changing data. Check the role first, then the data.
8. **SQL.** Every statement is prepared and bound with `Statement::bind`. User text is never concatenated into SQL.
   Dynamic SQL may only add fixed fragments that contain `?` placeholders.
9. **Passwords.** Only `IPasswordHasher` handles passwords. Passwords are never logged or stored in clear text.
10. **Functions.** Aim for cyclomatic complexity of 10 or less and about 50 lines or less. Split long functions.
11. **Tests.** Every new rule or bug fix comes with an automated test. The business-logic layer
    (`domain`, `services`, `common`) must keep at least 70% line coverage.
12. **Comments.** Comment why, not what. Public interfaces get a one-line description.
13. **Static analysis.** `cppcheck` (warning, style, performance, portability) must report nothing for `src/`.
