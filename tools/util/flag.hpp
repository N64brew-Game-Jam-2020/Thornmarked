#pragma once
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace flag {

// =============================================================================
// ProgramArguments
// =============================================================================

// The arguments passed to a program.
class ProgramArguments {
    int m_pos;
    int m_argc;
    char **m_argv;

public:
    // Create an invalid program arguments object.
    ProgramArguments() = default;

    // Create a program arguments object.
    ProgramArguments(int argc, char **argv)
        : m_pos{0}, m_argc{argc}, m_argv{argv} {}

    // Get the current argument.  Return nullptr if and only if the end is
    // reached.
    const char *arg() { return m_argv[m_pos]; }

    // Advance to the next argument.  Must not be called after the end is
    // reached.
    void Next() { m_pos++; }

    // Get the number of remaining arguments.
    int argc() const { return m_argc - m_pos; }

    // Get the remaining arguments.
    char **argv() const { return m_argv + m_pos; }

    // Return true if there are no more arguments in the set.
    bool empty() const { return m_pos >= m_argc; }
};

// =============================================================================
// Exception
// =============================================================================

class UsageError : public std::runtime_error {
public:
    explicit UsageError(const std::string &what);
    explicit UsageError(const char *what);
};

// =============================================================================
// Flags
// =============================================================================

// Whether an argument for the flag is required or possible.
enum class FlagArgument {
    None,
    Optional,
    Required,
};

// Base class for command-line flags.
class FlagBase {
public:
    virtual ~FlagBase();
    virtual FlagArgument Argument() const = 0;
    virtual void Parse(std::optional<std::string_view> arg) = 0;
};

// String valued flag.
class String : public FlagBase {
    std::string *m_ptr;

public:
    explicit String(std::string *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Integer valued flag.
class Int : public FlagBase {
    int *m_ptr;

public:
    explicit Int(int *ptr) : m_ptr{ptr} {}

    FlagArgument Argument() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Float32 valued flag.
class Float32 : public FlagBase {
    float *m_ptr;

public:
    explicit Float32(float *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Float64 valued flag.
class Float64 : public FlagBase {
    double *m_ptr;

public:
    explicit Float64(double *value) : m_ptr{value} {}

    FlagArgument Argument() const override;
    void Parse(std::optional<std::string_view> arg) override;
};

// Flag which sets a variable to a value if a flag appears.
template <typename T>
class SetValue : public FlagBase {
    T *m_ptr;
    T m_value;

public:
    SetValue(T *ptr, T value) : m_ptr{ptr}, m_value{value} {}
    ~SetValue() = default;

    FlagArgument Argument() const override { return FlagArgument::None; }
    void Parse(std::optional<std::string_view> arg) override {
        (void)&arg;
        *m_ptr = m_value;
    }
};

// =============================================================================
// Parser
// =============================================================================

// Command-line argument parser.
class Parser {
    struct Flag {
        std::shared_ptr<FlagBase> flag;
        std::string help;
        std::string metavar;
    };

    std::unordered_map<std::string, std::shared_ptr<const Flag>> m_flags;

public:
    ~Parser();

    // Add a flag to the argument parser.
    template <typename F>
    void AddFlag(F &&flag, const char *name, const char *help,
                 const char *metavar = nullptr) {
        static_assert(std::is_base_of<FlagBase, F>::value,
                      "flagmust be derived from FlagBase");
        AddFlagImpl(std::make_shared<F>(std::forward<F>(flag)), name, help,
                    metavar);
    }

    // Add a boolean-valued flag to the argument parser.
    //
    // The value will be true for -flag, -flag=true, flag=yes, flag=on, and
    // -flag=1. The value will be false for -no-flag, -flag=false, -flag=no,
    // flag=off, and -flag=0.
    void AddBoolFlag(bool *value, const char *name, const char *help);

    void ParseAll(ProgramArguments &args);
    void ParseNext(ProgramArguments &args);

private:
    void AddFlagImpl(std::shared_ptr<FlagBase> flag, const char *name,
                     const char *help, const char *metavar);
};

} // namespace flag
