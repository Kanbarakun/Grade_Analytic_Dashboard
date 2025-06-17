                                               // GROUP 1 // 1ST YEAR PROJECT // GRADE_ANALYTIC_DASHBOARD //

#include <iostream>          // For standard input/output streams
#include <iomanip>           // For I/O manipulators (e.g., setw, setprecision)
#include <vector>            // For using the vector container
#include <string>            // For string operations
#include <algorithm>         // For algorithms like transform, min_element, etc.
#include <ctime>             // For date/time functions
#include <sstream>           // For string stream operations
#include <stdexcept>         // For standard exception classes
#include <limits>            // For numeric limits (e.g., max() for input validation)
#include <cctype>            // For character handling (e.g., isalpha, isspace)

// MySQL Connector/C++ 9.x JDBC headers
// These headers are required for connecting and interacting with MySQL database
#include <jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/cppconn/driver.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/prepared_statement.h>

using namespace std;

// To clearscreen function
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// student structure
struct Student {
    int id = 0;
    string name = "";
    string section = "";
    double math = 0.0;
    double science = 0.0;
    double english = 0.0;
    double average = 0.0;
    string remarks = "";
    string created_at = "";
    string updated_at = "";

    Student() = default;
};

// Returns the current date and time as a timestamp string (format: YYYY-MM-DD HH:MM:SS)
string getCurrentTimestamp() {
    time_t now = time(nullptr);
    tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return string(buffer);
}

// Returns remarks string based on a numeric average grade
string calculateRemarks(double grade) {
    if (grade >= 90) return "Excellent";
    if (grade >= 75) return "Good";
    return "Needs Improvement";
}

// Converts a string to all lowercase letters
string tolowercase(const string& str) {
    string lowstr = str;
    transform(lowstr.begin(), lowstr.end(), lowstr.begin(), ::tolower);
    return lowstr;
}

// Validates if the given name contains only alphabetic characters and spaces
bool isValidName(const string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!isalpha(static_cast<unsigned char>(c)) && (!isspace(static_cast<unsigned char>(c)))) {
            return false;
        }
    }
    return true;
}

// Prompts user for a valid integer and keeps asking until the input is correct
// Returns only an integer value, rejects decimals and non-integers
int ValidInput(const string& prompt) {
    string input;
    int value;
    while (true) {
        cout << prompt;
        getline(cin, input);

        // Check if input contains a decimal point
        if (input.find('.') != string::npos) {
            cout << "Invalid! Please enter a whole number (no decimals).\n";
            continue;
        }

        // Check if input is all digits (and not empty)
        bool isNumber = !input.empty() && all_of(input.begin(), input.end(), ::isdigit);

        if (isNumber) {
            value = stoi(input);
            break;
        }
        else {
            cout << "Invalid! Please enter a valid number.\n";
        }
    }
    return value;
}

// Prompts user for a valid grade (double in range 0 to 100) and keeps asking until valid
double ValidGrade(const string& subject) {
    double grade;
    while (true) {
        cout << "Enter " << subject << " Grade (0 - 100, decimals allowed): ";
        cin >> grade;
        if (!cin.fail() && grade >= 0 && grade <= 100) break;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid! Grade must be between 0 - 100\n";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return grade;
}

// MySQL connection configuration
struct DatabaseConfig {
    string host = "127.0.0.1";
    int port = 3306;
    string user = "root";
    string password = "";
    string database = "grades_dashboard";
};

// Global pointers for the MySQL driver and connection
sql::mysql::MySQL_Driver* driver = nullptr;
unique_ptr<sql::Connection> con;

// Tries to connect to a MySQL server using the given config; returns true if successful
bool testConnection(const DatabaseConfig& config) {
    try {
        string connectionString = "tcp://" + config.host + ":" + to_string(config.port);
        driver = sql::mysql::get_mysql_driver_instance();
        unique_ptr<sql::Connection> testCon(driver->connect(connectionString, config.user, config.password));
        testCon->setSchema(config.database);
        cout << "✓ Connection test successful on " << connectionString << endl;
        return true;
    }
    catch (sql::SQLException& e) {
        cout << "✗ Connection failed on tcp://" << config.host << ":" << config.port
            << " - " << e.what() << endl;
        return false;
    }
}

// Attempts all known connection configs until one works, connects and creates table if needed
static void connectDB() {
    cout << "\n=== ATTEMPTING DATABASE CONNECTION ===" << endl;

    vector<DatabaseConfig> configs = {
        {"127.0.0.1", 3306, "root", "", "grades_dashboard"},  // Standard port
        {"127.0.0.1", 3307, "root", "", "grades_dashboard"},  // Alternative port
        {"localhost", 3306, "root", "", "grades_dashboard"},  // localhost
        {"127.0.0.1", 3308, "root", "", "grades_dashboard"},  // XAMPP default
    };

    DatabaseConfig workingConfig;
    bool connected = false;

    for (const auto& config : configs) {
        cout << "Trying " << config.host << ":" << config.port << "..." << endl;
        if (testConnection(config)) {
            workingConfig = config;
            connected = true;
            break;
        }
    }

    if (!connected) {
        cout << "\n=== TROUBLESHOOTING GUIDE ===" << endl;
        cout << "1. Make sure MySQL/XAMPP is running" << endl;
        cout << "2. Check MySQL service in Services (services.msc)" << endl;
        cout << "3. Try: net start mysql (as Administrator)" << endl;
        cout << "4. Verify credentials in MySQL Workbench" << endl;
        cout << "5. Create database: CREATE DATABASE grades_dashboard;" << endl;
        cout << "\nPress any key to exit..." << endl;
        cin.get();
        exit(1);
    }

    try {
        string connectionString = "tcp://" + workingConfig.host + ":" + to_string(workingConfig.port);
        driver = sql::mysql::get_mysql_driver_instance();
        con.reset(driver->connect(connectionString, workingConfig.user, workingConfig.password));
        con->setSchema(workingConfig.database);

        // Create table if it doesn't exist (with average column)
        unique_ptr<sql::Statement> stmt(con->createStatement());
        stmt->execute(R"(
            CREATE TABLE IF NOT EXISTS students (
                id INT AUTO_INCREMENT PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                section VARCHAR(50),
                math DOUBLE DEFAULT 0,
                science DOUBLE DEFAULT 0,
                english DOUBLE DEFAULT 0,
                average DOUBLE DEFAULT 0,
                remarks VARCHAR(50),
                created_at DATETIME,
                updated_at DATETIME
            )
        )");

        // Ensure average column exists
        stmt->execute("ALTER TABLE students ADD COLUMN IF NOT EXISTS average DOUBLE DEFAULT 0");

        cout << "✓ Connected to MySQL database successfully!" << endl;
        cout << "✓ Table 'students' ready" << endl;
        cout << "========================================\n" << endl;
    }
    catch (sql::SQLException& e) {
        cerr << "Final connection error: " << e.what() << endl;
        cerr << "Error Code: " << e.getErrorCode() << endl;
        exit(1);
    }
}

// Closes the MySQL database connection if open
void disconnectDB() {
    if (con) {
        con.reset();
        cout << "Database connection closed." << endl;
    }
}

// CRUD Functions
// Prompts for student info and adds a new student record to the database
void addStudent() {
    Student s;
    cout << "\n=== ADD NEW STUDENT ===" << endl;
    cout << "Enter student name: ";
    getline(cin, s.name);
    while (!isValidName(s.name)) {
        cout << "Please enter a proper name (letters and spaces only).\n";
        cout << "Enter student name: ";
        getline(cin, s.name);
    }

    cout << "Enter section: ";
    getline(cin, s.section);
    if (s.section.empty()) s.section = "N/A";

    s.math = ValidGrade("Math");
    s.science = ValidGrade("Science");
    s.english = ValidGrade("English");
    s.average = (s.math + s.science + s.english) / 3.0;
    s.remarks = calculateRemarks(s.average);
    s.created_at = getCurrentTimestamp();
    s.updated_at = s.created_at;

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "INSERT INTO students (name, section, math, science, english, average, remarks, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
        ));
        pstmt->setString(1, s.name);
        pstmt->setString(2, s.section);
        pstmt->setDouble(3, s.math);
        pstmt->setDouble(4, s.science);
        pstmt->setDouble(5, s.english);
        pstmt->setDouble(6, s.average);
        pstmt->setString(7, s.remarks);
        pstmt->setString(8, s.created_at);
        pstmt->setString(9, s.updated_at);
        pstmt->execute();
        cout << "✓ Student added successfully!\n";
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Displays all students in the database with their details
void viewStudents() {
    try {
        unique_ptr<sql::Statement> stmt(con->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM students ORDER BY id"));

        cout << "\n=== STUDENT RECORDS ===" << endl;
        cout << string(132, '-') << endl;
        cout << left << setw(5) << "ID"
            << setw(20) << "Name"
            << setw(15) << "Section"
            << setw(8) << "Math"
            << setw(10) << "Science"
            << setw(10) << "English"
            << setw(12) << "Average"
            << setw(20) << "Remarks"
            << setw(20) << "Created At" << endl;
        cout << string(132, '-') << endl;

        bool hasRecords = false;
        while (res->next()) {
            hasRecords = true;
            cout << left << setw(5) << res->getInt("id")
                << setw(20) << (res->isNull("name") ? "[NULL]" : res->getString("name"))
                << setw(15) << (res->isNull("section") ? "[NULL]" : res->getString("section"))
                << setw(8) << fixed << setprecision(1) << res->getDouble("math")
                << setw(10) << res->getDouble("science")
                << setw(10) << res->getDouble("english")
                << setw(12) << res->getDouble("average")
                << setw(20) << (res->isNull("remarks") ? "[NULL]" : res->getString("remarks"))
                << setw(20) << (res->isNull("created_at") ? "[NULL]" : res->getString("created_at")) << endl;
        }

        if (!hasRecords) {
            cout << "No student records found." << endl;
        }
        cout << string(132, '-') << endl;
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Prompts for a student name, then displays all matching students
void searchStudent() {
    string searchName;
    cout << "\n=== SEARCH STUDENT ===" << endl;
    cout << "Enter student name to search: ";
    getline(cin, searchName);
    while (!isValidName(searchName)) {
        cout << "Please enter a proper name.\n";
        cout << "Enter student name to search: ";
        getline(cin, searchName);
    }

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT * FROM students WHERE LOWER(name) LIKE ? ORDER BY name"
        ));
        string pattern = "%" + tolowercase(searchName) + "%";
        pstmt->setString(1, pattern);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        cout << "\n--- Search Results for \"" << searchName << "\" ---" << endl;
        cout << string(132, '-') << endl;
        cout << left << setw(5) << "ID"
            << setw(20) << "Name"
            << setw(15) << "Section"
            << setw(8) << "Math"
            << setw(10) << "Science"
            << setw(10) << "English"
            << setw(12) << "Average"
            << setw(20) << "Remarks"
            << setw(20) << "Created At" << endl;
        cout << string(132, '-') << endl;

        bool found = false;
        while (res->next()) {
            found = true;
            cout << left << setw(5) << res->getInt("id")
                << setw(20) << res->getString("name")
                << setw(15) << res->getString("section")
                << setw(8) << fixed << setprecision(1) << res->getDouble("math")
                << setw(10) << res->getDouble("science")
                << setw(10) << res->getDouble("english")
                << setw(12) << res->getDouble("average")
                << setw(20) << res->getString("remarks")
                << setw(20) << res->getString("created_at") << endl;
        }

        if (!found) {
            cout << "No students found with the name containing \"" << searchName << "\"." << endl;
        }
        cout << string(132, '-') << endl;
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Prompts for a section name, then displays all students in that section
void searchSection() {
    string searchSection;
    cout << "\n=== SEARCH BY SECTION ===" << endl;
    cout << "Enter section to search: ";
    getline(cin, searchSection);

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT * FROM students WHERE LOWER(section) = ? ORDER BY name"
        ));
        pstmt->setString(1, tolowercase(searchSection));
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        cout << "\n--- Students in Section \"" << searchSection << "\" ---" << endl;
        cout << string(132, '-') << endl;
        cout << left << setw(5) << "ID"
            << setw(20) << "Name"
            << setw(15) << "Section"
            << setw(8) << "Math"
            << setw(10) << "Science"
            << setw(10) << "English"
            << setw(12) << "Average"
            << setw(20) << "Remarks"
            << setw(20) << "Created At" << endl;
        cout << string(132, '-') << endl;

        bool found = false;
        while (res->next()) {
            found = true;
            cout << left << setw(5) << res->getInt("id")
                << setw(20) << res->getString("name")
                << setw(15) << res->getString("section")
                << setw(8) << fixed << setprecision(1) << res->getDouble("math")
                << setw(10) << res->getDouble("science")
                << setw(10) << res->getDouble("english")
                << setw(12) << res->getDouble("average")
                << setw(20) << res->getString("remarks")
                << setw(20) << res->getString("created_at") << endl;
        }

        if (!found) {
            cout << "No students found in section \"" << searchSection << "\"." << endl;
        }
        cout << string(132, '-') << endl;
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Prompts for a student name, displays current info, then allows updating of the record
void updateStudent() {
    string name;
    cout << "\n=== UPDATE STUDENT ===" << endl;
    cout << "Enter student name to update: ";
    getline(cin, name);
    while (!isValidName(name)) {
        cout << "Please enter a proper name.\n";
        cout << "Enter student name to update: ";
        getline(cin, name);
    }

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT * FROM students WHERE LOWER(name) = ?"
        ));
        pstmt->setString(1, tolowercase(name));
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (res->next()) {
            int id = res->getInt("id");
            cout << "\n--- Current Record ---" << endl;
            cout << "ID: " << id << endl;
            cout << "Name: " << res->getString("name") << endl;
            cout << "Section: " << res->getString("section") << endl;
            cout << "Math: " << res->getDouble("math") << endl;
            cout << "Science: " << res->getDouble("science") << endl;
            cout << "English: " << res->getDouble("english") << endl;
            cout << "Average: " << res->getDouble("average") << endl;
            cout << "Remarks: " << res->getString("remarks") << endl;

            string newName, newSection;
            cout << "\n--- Enter New Information ---" << endl;
            cout << "Enter new name (or press Enter to keep current): ";
            getline(cin, newName);
            if (newName.empty()) newName = res->getString("name");

            cout << "Enter new section (or press Enter to keep current): ";
            getline(cin, newSection);
            if (newSection.empty()) newSection = res->getString("section");

            cout << "Current grades will be updated. Enter new grades:" << endl;
            double newMath = ValidGrade("Math");
            double newScience = ValidGrade("Science");
            double newEnglish = ValidGrade("English");
            double newAverage = (newMath + newScience + newEnglish) / 3.0;
            string newRemarks = calculateRemarks(newAverage);
            string updatedAt = getCurrentTimestamp();

            unique_ptr<sql::PreparedStatement> updateStmt(con->prepareStatement(
                "UPDATE students SET name=?, section=?, math=?, science=?, english=?, average=?, remarks=?, updated_at=? WHERE id=?"
            ));
            updateStmt->setString(1, newName);
            updateStmt->setString(2, newSection);
            updateStmt->setDouble(3, newMath);
            updateStmt->setDouble(4, newScience);
            updateStmt->setDouble(5, newEnglish);
            updateStmt->setDouble(6, newAverage);
            updateStmt->setString(7, newRemarks);
            updateStmt->setString(8, updatedAt);
            updateStmt->setInt(9, id);

            updateStmt->execute();
            cout << "✓ Student updated successfully!" << endl;
        }
        else {
            cout << "Student not found." << endl;
        }
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Prompts for a student name, displays info, asks for confirmation, and deletes the record if confirmed
void deleteStudent() {
    string name;
    cout << "\n=== DELETE STUDENT ===" << endl;
    cout << "Enter student name to delete: ";
    getline(cin, name);

    try {
        // First show the student to be deleted
        unique_ptr<sql::PreparedStatement> selectStmt(con->prepareStatement(
            "SELECT * FROM students WHERE LOWER(name) = ?"
        ));
        selectStmt->setString(1, tolowercase(name));
        unique_ptr<sql::ResultSet> res(selectStmt->executeQuery());

        if (res->next()) {
            cout << "\n--- Student to be deleted ---" << endl;
            cout << "Name: " << res->getString("name") << endl;
            cout << "Section: " << res->getString("section") << endl;
            cout << "Math: " << res->getDouble("math") << endl;
            cout << "Science: " << res->getDouble("science") << endl;
            cout << "English: " << res->getDouble("english") << endl;
            cout << "Average: " << res->getDouble("average") << endl;

            char confirm;
            cout << "\nAre you sure you want to delete this student? (Y/N): ";
            cin >> confirm;
            cin.ignore(numeric_limits<streamsize>::max(), '\n');

            if (toupper(confirm) == 'Y') {
                unique_ptr<sql::PreparedStatement> deleteStmt(con->prepareStatement(
                    "DELETE FROM students WHERE LOWER(name) = ?"
                ));
                deleteStmt->setString(1, tolowercase(name));
                int affected = deleteStmt->executeUpdate();

                if (affected > 0) {
                    cout << "✓ Student deleted successfully!" << endl;
                }
                else {
                    cout << "Failed to delete student." << endl;
                }
            }
            else {
                cout << "Delete operation cancelled." << endl;
            }
        }
        else {
            cout << "Student not found." << endl;
        }
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Calculates the mean (average) of a vector of grades
double calculateMean(const vector<double>& grades) {
    if (grades.empty()) return 0;
    double sum = 0;
    for (double g : grades) sum += g;
    return sum / grades.size();
}

// Finds the maximum value in a vector of grades
double findMax(const vector<double>& grades) {
    if (grades.empty()) return 0;
    return *max_element(grades.begin(), grades.end());
}

// Finds the minimum value in a vector of grades
double findMin(const vector<double>& grades) {
    if (grades.empty()) return 0;
    return *min_element(grades.begin(), grades.end());
}

// Displays analytics for all students: highest, lowest, average per subject, and performance distribution
void displayAnalytics() {
    try {
        unique_ptr<sql::Statement> stmt(con->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT math, science, english, average FROM students"));

        vector<double> math, science, english, averages;
        while (res->next()) {
            double m = res->getDouble("math");
            double s = res->getDouble("science");
            double e = res->getDouble("english");
            double a = res->getDouble("average");
            math.push_back(m);
            science.push_back(s);
            english.push_back(e);
            averages.push_back(a);
        }

        cout << "\n=== GRADE ANALYTICS DASHBOARD ===" << endl;
        if (!math.empty()) {
            cout << string(80, '=') << endl;
            cout << left << setw(12) << "Subject"
                << setw(12) << "Highest"
                << setw(12) << "Lowest"
                << setw(12) << "Average"
                << setw(12) << "Students" << endl;
            cout << string(80, '-') << endl;

            cout << left << setw(12) << "Math"
                << setw(12) << fixed << setprecision(1) << findMax(math)
                << setw(12) << findMin(math)
                << setw(12) << calculateMean(math)
                << setw(12) << math.size() << endl;

            cout << left << setw(12) << "Science"
                << setw(12) << findMax(science)
                << setw(12) << findMin(science)
                << setw(12) << calculateMean(science)
                << setw(12) << science.size() << endl;

            cout << left << setw(12) << "English"
                << setw(12) << findMax(english)
                << setw(12) << findMin(english)
                << setw(12) << calculateMean(english)
                << setw(12) << english.size() << endl;

            cout << string(80, '-') << endl;
            cout << left << setw(12) << "Overall"
                << setw(12) << findMax(averages)
                << setw(12) << findMin(averages)
                << setw(12) << calculateMean(averages)
                << setw(12) << averages.size() << endl;
            cout << string(80, '=') << endl;

            // Performance distribution
            int excellent = 0, good = 0, needsImprovement = 0;
            for (double avg : averages) {
                if (avg >= 90) excellent++;
                else if (avg >= 75) good++;
                else needsImprovement++;
            }

            cout << "\n--- Performance Distribution ---" << endl;
            cout << "Excellent (90+): " << excellent << " students" << endl;
            cout << "Good (75-89): " << good << " students" << endl;
            cout << "Needs Improvement (<75): " << needsImprovement << " students" << endl;
        }
        else {
            cout << "No student data available for analytics." << endl;
        }
    }
    catch (sql::SQLException& e) {
        cerr << "MySQL error: " << e.what() << endl;
    }
}

// Main menu loop: connects to DB, displays menu, dispatches to functions, and closes DB on exit
int main() {
    cout << "=== GRADE ANALYTICS DASHBOARD ===" << endl;
    cout << "Initializing database connection..." << endl;

    connectDB();

    int choice;
    do {
        clearScreen();
        cout << "\n" << string(50, '=') << endl;
        cout << "         GRADE ANALYTICS DASHBOARD" << endl;
        cout << string(50, '=') << endl;
        cout << "1. Add Student" << endl;
        cout << "2. View All Students" << endl;
        cout << "3. Update Student" << endl;
        cout << "4. Delete Student" << endl;
        cout << "5. Search by Section" << endl;
        cout << "6. Search Student" << endl;
        cout << "7. View Analytics" << endl;
        cout << "8. Exit" << endl;
        cout << string(50, '=') << endl;

        choice = ValidInput("Enter your choice (1-8): ");

        switch (choice) {
        case 1: addStudent(); break;
        case 2: viewStudents(); break;
        case 3: updateStudent(); break;
        case 4: deleteStudent(); break;
        case 5: searchSection(); break;
        case 6: searchStudent(); break;
        case 7: displayAnalytics(); break;
        case 8:
            cout << "\nExiting Grade Analytics Dashboard..." << endl;
            cout << "Thank you for using the system!" << endl;
            break;
        default:
            cout << "Invalid choice. Please select 1-8." << endl;

        }

        if (choice != 8) {
            cout << "\nPress Enter to continue...";
            cin.get();
        }

    } while (choice != 8);

    disconnectDB();
    return 0;
}
