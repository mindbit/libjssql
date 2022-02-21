# Coding Style for libjssql

- Use only tabs for indentation, tabs should be 8 characters wide. Tabs must never follow spaces.
- Do not add trailing whitespaces at the end of a line or at the end of a file. Configure your editor to make them visible.
- Do not use comments like “//”. Use Doxygen comments.
- Try to avoid more than 3 levels of imbrication.
- Opening curly brackets must be on the same line as the preceding statement, separated by a space character. As an exception, function opening brackets must be on a separate line.
- Using curly brackets for single instruction blocks is discouraged.
- Inside a switch block, case statements must be aligned with the switch statement.
- Snake case. An exception to this rule are the native C functions that are used to define JS object properties.
- Do not mix code and variable declarations. Variable declarations should be grouped at the beginning of the block. Use a blank line to separate variable declarations from code.
- Use [checkpatch](https://www.kernel.org/doc/html/latest/dev-tools/checkpatch.html) to validate coding style.

### Git
Make sure each commit corresponds to **one** code / content change only.
If there are multiple commits belonging to a given change, please squash the commits.

Use commit messages with verbs at imperative mood: “Add README” “Update contents” “Introduce feature”.
How a good commit message should look like: https://cbea.ms/git-commit/
