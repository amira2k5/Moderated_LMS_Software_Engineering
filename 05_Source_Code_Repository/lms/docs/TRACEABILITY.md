# Requirements traceability matrix

Each functional requirement is linked to its use case, design elements, source files and automated tests (suite.test). Source files are in `src/`.

| Req. | Pri. | Use case | Design elements | Source | Tests | Result |
|---|---|---|---|---|---|---|
| FR-01 | Must | UC01 | AuthService, User::allows(), ConsoleApp | services.cpp, domain.cpp, ui.cpp | Auth.LoginSucceedsAndIsCaseInsensitiveOnUsername; Auth.WrongPasswordAndUnknownUserGiveTheSameMessage; Auth.DeactivatedAccountCannotLogIn; Domain.PermissionMatrixPerRole; Console.FailedLoginShowsAPlainMessageAndQuitWorks | All pass |
| FR-02 | Must | UC03 | AdminService::createUser, UserFactory, user_account | services.cpp, domain.cpp, data.cpp | Admin.CreateUserValidatesEveryField; Admin.HeadOfDepartmentRules; Domain.UserFactoryBuildsEachRoleAndValidates; Console.AdministratorCreatesAnAccount | All pass |
| FR-03 | Must | UC04 | AdminService, department, course | services.cpp, data.cpp | Admin.DepartmentsCoursesEnrolmentsAndAssignments; Data.RepositoriesRoundTripEntities | All pass |
| FR-04 | Must | UC05 | AdminService::enrol, assignLecturer; enrolment, teaching | services.cpp, data.cpp | Admin.DepartmentsCoursesEnrolmentsAndAssignments | All pass |
| FR-05 | Must | UC07 | AssignmentService::createAssignment; assignment | services.cpp, data.cpp | AssignmentSvc.CreateAssignmentValidation | All pass |
| FR-06 | Must | UC17 | AssignmentService::coursesFor, assignmentsFor | services.cpp, ui.cpp | AssignmentSvc.ListingsRespectRoles; CR1.StudentsSeeTheirExtensionOnTheAssignmentList; CR1.ConsoleShowsNoExtensionTextWhenThereIsNone; Console.StudentSubmitsWork | All pass |
| FR-07 | Must | UC18 | SubmissionService::submit, LocalFileStore; submission, submission_version | services.cpp, data.cpp | Submission.OnTimeFirstSubmission; Submission.InvalidFilesAreRejectedAndStoreNothing; Submission.OnlyEnrolledStudentsMaySubmit; UC18.Main_OnTimeSubmission; UC18.E1_StudentNotEnrolled; UC18.E2_InvalidFile; UC18.E5_StorageErrorLeavesNoPartialVersion; Submission.DatabaseFailureLeavesNoPartialSubmission | All pass |
| FR-08 | Must | UC19 | SubmissionService::submit (resubmission path); submission_version | services.cpp | Submission.ResubmissionBeforeDeadlineKeepsAllVersions; Submission.CannotResubmitAfterDeadlineOrAfterLateOrAfterMarking; UC18.A1_ResubmissionBeforeDeadline; UC18.E4_SubmissionAfterMarkingRefused | All pass |
| FR-09 | Must | UC20 | SubmissionService::submit (late path); state Late | services.cpp | Submission.DeadlineBoundaries; UC18.A2_LateSubmissionIsAcceptedAndFlagged; UC18.A2_SubmissionAfterSeventyTwoHoursIsStillAccepted; UC18.E3_ResubmissionOfLateWorkRefused; Console.LateSubmissionWarnsAboutPenalty | All pass |
| FR-10 | Must | UC08 | AssignmentService::grantExtension; extension | services.cpp, data.cpp | AssignmentSvc.ExtensionRules; Data.ExtensionIsUpsertedPerStudentAndAssignment; Submission.ExtensionMovesTheEffectiveDeadline; Marking.ExtensionRemovesThePenalty | All pass |
| FR-11 | Must | UC11 | ILatePenaltyPolicy, TenPercentPerDayPolicy | domain.cpp | Penalty.BoundaryValuesForDeadlineAndDays; Penalty.MarkBoundariesAndFloorAtZero; Penalty.RoundsHalfUpToTwoDecimals; Penalty.PolicyIsReplaceable; Marking.LatePenaltyIsApplied; UC10.Main_LatePenaltyShownAndApplied | All pass |
| FR-12 | Must | UC09, UC10 | MarkingService::mark, remark; AssignmentService::listSubmissions; mark | services.cpp, data.cpp | Marking.CannotMarkBeforeTheEffectiveDeadline; Marking.OnTimeMarkHasNoPenaltyAndWritesAnAuditEntry; Marking.RejectsInvalidMarksFeedbackAndWrongPeople; UC10.Main_MarkAfterDeadlineHiddenFromStudent; UC10.E1_MarkOutsideRange; UC10.E2_FeedbackMissing; UC10.E3_DeadlineNotYetPassed; UC10.E4_ModeratedOrReleasedMarkCannotBeChangedByLecturer; Console.LecturerMarksAndHodModeratesAndReleases | All pass |
| FR-13 | Must | UC12 | MarkChangePublisher, AuditObserver; audit_log (append-only triggers) | domain.cpp, services.cpp, data.cpp | Data.AuditLogIsAppendOnly; Marking.RemarkRequiresReasonAndIsAudited; Marking.FailedAuditRollsBackTheMark; Moderation.AdjustMarkBeforeApprovalIsAuditedAndValidated; UC10.A1_RemarkWithReason; UC10.E5_DatabaseErrorLeavesNothingSaved | All pass |
| FR-14 | Must | UC13 | ModerationService::listForModeration | services.cpp | Moderation.ReviewIsLimitedToOwnDepartment; UC14.E2_CourseOutsideDepartment | All pass |
| FR-15 | Must | UC14, UC15 | ModerationService::approve, adjustMark; state Moderated | services.cpp | Moderation.AdjustMarkBeforeApprovalIsAuditedAndValidated; Moderation.ApproveAndReleaseBatchesSkipWrongStates; UC14.A1_AdjustAMark; UC14.E1_UnmarkedSubmissionIsSkipped; UC14.E3_ReasonMissingOrMarkOutOfRange | All pass |
| FR-16 | Must | UC14 | ModerationService::release; state Released | services.cpp | Moderation.ReleaseRequiresApprovalFirst; Moderation.BatchIsAllOrNothing; UC14.Main_ApproveThenRelease; UC14.A2_PartialRelease; UC14.E4_BatchRolledBackOnDatabaseError | All pass |
| FR-17 | Must | UC21 | SubmissionService::releasedMark | services.cpp | Moderation.StudentsNeverSeeUnreleasedMarks; Console.StudentSeesNothingBeforeRelease; UC10.Main_MarkAfterDeadlineHiddenFromStudent | All pass |
| FR-18 | Must | all | requireAllowed(), User::allows() | services.cpp, domain.cpp | Domain.PermissionMatrixPerRole; AccessControl.EachRoleIsLimitedToItsOwnFunctions; AccessControl.DeactivatedUserCannotPerformActions | All pass |
| FR-19 | Should | UC14 | ModerationService::correctReleasedMark | services.cpp | Moderation.CorrectionOfAReleasedMarkIsAuditedAndVisible; UC14.A3_CorrectAReleasedMark | All pass |
| FR-20 | Should | UC16 | AuditService::list; audit_log | services.cpp, data.cpp | Moderation.AuditLogAccessIsRoleBased; Data.AuditFilterByDepartmentUserAndTime; Console.ChangePasswordAndAuditLogScreens | All pass |
| FR-21 | Should | UC02 | AuthService::changePassword, lockout | services.cpp | Auth.AccountLocksAfterFiveFailuresAndUnlocksAfterFiveMinutes; Auth.SuccessfulLoginResetsTheFailureCounter; Auth.ChangePasswordChecksOldPasswordAndRules; Console.ChangePasswordAndAuditLogScreens | All pass |
| FR-22 | Should | UC06 | AdminService::backup, Database::backupTo | services.cpp, data.cpp | Data.BackupCreatesAnOpenableCopy | All pass |
| FR-23 | Could | - | (CSV export) not built | - | - | Descoped (Could have) |
| FR-24 | Must (CR1) | UC22 | AssignmentService::grantClassExtension; extension rows | services.cpp | CR1.ClassExtensionGivesEveryEnrolledStudentTheNewDeadline; CR1.NeverShortensALaterIndividualExtension; CR1.ExtendsAnEarlierIndividualExtension; CR1.ClassExtensionRemovesThePenaltyWhenMarking; CR1.ValidationAndAccessRules; CR1.ClassExtensionIsAllOrNothing; CR1.ConsoleLecturerGrantsAClassExtensionAndStudentsSeeIt; CR1.ConsoleReportsStudentsWhoKeepALaterExtension | All pass |

## Non-functional requirements

| Req. | Pri. | Method | Evidence | Result achieved | Status |
|---|---|---|---|---|---|
| NFR-01 | Must | Test | Performance.LoginListingAndSavingMeetTheTargetsWithEightHundredStudents | Login 78 ms with 800 students and 60 staff stored (target 2,000 ms) | Met |
| NFR-02 | Must | Test | Performance.LoginListingAndSavingMeetTheTargetsWithEightHundredStudents | Listing 150 submissions 0.5 ms; saving a mark 2.4 ms; class extension for 800 students 32 ms (target 1,000 ms) | Met |
| NFR-03 | Must | Inspection | (no test: nothing to run) | No networking calls exist in src/ (search for socket, curl, http, connect, getaddrinfo found nothing); every test runs without a network | Met |
| NFR-04 | Must | Test, inspection | Auth.PasswordsAreStoredAsSaltedHashesOnly | Argon2id hashes; same password gives different hashes; no stored hash contains the clear password | Met |
| NFR-05 | Must | Inspection, test | Data.QueriesAreParameterisedAgainstInjection | All 40 statements in data.cpp are prepared and bound; no SQL outside data.cpp and schema.cpp; injection text is stored as data | Met |
| NFR-06 | Must | Test | Robustness.AtLeastFiftyInvalidInputsAreRejectedCleanly | 60 invalid inputs, a damaged database, bad console input and end of input all produce a plain error or a clean exit, no crash | Met |
| NFR-07 | Must | Test | Data.AuditLogIsAppendOnly | Every mark change writes one audit entry (marking, re-marking, adjusting, correcting); update and delete on the log are refused by the database | Met |
| NFR-08 | Must | User test | (not run) | Needs three first-time lecturers using the manual; not possible without real users. Menu text was reviewed for plain language | Not yet verified |
| NFR-09 | Should | Inspection | Console tests | Status and errors are text; no colour; keyboard only | Met |
| NFR-10 | Should | Test | Concurrency.FiveSessionsWriteToOneDatabaseFileWithoutCorruption | Five connections, 100 concurrent writes, no failure, integrity check ok (run 3 times) | Met |
| NFR-11 | Should | Measurement | All tests | Business-logic coverage 98.6% (target 70%); cppcheck reports nothing | Met |
| NFR-12 | Should | Demonstration | Data.BackupCreatesAnOpenableCopy | A backup was written and opened in under one second; restore is a file copy (procedure in docs/INSTALL.md) | Met (procedure to be rehearsed on site) |
