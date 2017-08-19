/*
 * A very bad streaming TOML parser
 */
#include "toml.h"
#include "debug.h"
#include <cassert>
#include <string>


struct Token
{
    enum class Type
    {
        Eof,
        SquareOpen,
        SquareClose,
        BraceOpen,
        BraceClose,
        Assign,
        Newline,
        Comma,
        Dot,

        Ident,
        String,
    };

    Type    m_type;
    ::std::string   m_data;

    static Token lex_from(::std::ifstream& is);
    static Token lex_from_inner(::std::ifstream& is);

    const ::std::string& as_string() const {
        assert(m_type == Type::Ident || m_type == Type::String);
        return m_data;
    }

    friend ::std::ostream& operator<<(::std::ostream& os, const Token& x) {
        switch(x.m_type)
        {
        case Type::Eof:   os << "Eof";    break;
        case Type::SquareOpen:  os << "SquareOpen"; break;
        case Type::SquareClose: os << "SquareClose"; break;
        case Type::BraceOpen:   os << "BraceOpen"; break;
        case Type::BraceClose:  os << "BraceClose"; break;
        case Type::Assign:      os << "Assign";   break;
        case Type::Newline:     os << "Newline";  break;
        case Type::Comma:       os << "Comma";    break;
        case Type::Dot:         os << "Dot";      break;
        case Type::Ident:  os << "Ident(" << x.m_data << ")";  break;
        case Type::String: os << "String(" << x.m_data << ")"; break;
        }
        return os;
    }
};

TomlFile::TomlFile(const ::std::string& filename):
    m_if(filename)
{
    if( !m_if.is_open() ) {
        throw ::std::runtime_error("Unable to open file '" + filename + "'");
    }
}
TomlFileIter TomlFile::begin()
{
    TomlFileIter rv { *this };
    ++rv;
    return rv;
}
TomlFileIter TomlFile::end()
{
    return TomlFileIter { *this };
}

TomlKeyValue TomlFile::get_next_value()
{
    auto t = Token::lex_from(m_if);

    if(m_current_composite.empty())
    {
        while( t.m_type == Token::Type::Newline )
            t = Token::lex_from(m_if);

        // Expect '[', a string, or an identifier
        switch(t.m_type)
        {
        case Token::Type::Eof:
            // Empty return indicates the end of the list
            return TomlKeyValue {};
        case Token::Type::SquareOpen:
            m_current_block.clear();
            do
            {
                t = Token::lex_from(m_if);
                bool is_array = false;
                if(t.m_type == Token::Type::SquareOpen)
                {
                    is_array = true;
                    t = Token::lex_from(m_if);
                }
                assert(t.m_type == Token::Type::Ident || t.m_type == Token::Type::String);
                m_current_block.push_back(t.as_string());
                if(is_array)
                {
                    m_current_block.push_back(::format(m_array_counts[t.as_string()]++));
                    t = Token::lex_from(m_if);
                    assert(t.m_type == Token::Type::SquareClose);
                }

                t = Token::lex_from(m_if);
            } while(t.m_type == Token::Type::Dot);
            if( t.m_type != Token::Type::SquareClose )
            {
                throw "";
            }
            t = Token::lex_from(m_if);
            if (t.m_type != Token::Type::Newline)
            {
                throw "";
            }
            DEBUG("Start block " << m_current_block);
            // TODO: Are empty sections allowed?
            //goto recurse;

            t = Token::lex_from(m_if);
            break ;
        default:
            break;
        }
    }
    else
    {
        // Expect a string or an identifier
        if( t.m_type == Token::Type::Eof )
        {
            // EOF isn't allowed here
            throw "";
        }
    }
    switch (t.m_type)
    {
    case Token::Type::String:
    case Token::Type::Ident:
        break;
    default:
        throw "";
    }
    ::std::string   key_name = t.as_string();
    t = Token::lex_from(m_if);

    if(t.m_type != Token::Type::Assign)
        throw "";
    t = Token::lex_from(m_if);

    TomlKeyValue    rv;
    switch(t.m_type)
    {
    case Token::Type::String:
        rv.path = m_current_block;
        rv.path.insert(rv.path.begin(), m_current_composite.begin(), m_current_composite.end());
        rv.path.push_back(key_name);

        rv.value = TomlValue { t.m_data };
        break;
    case Token::Type::SquareOpen:
        rv.path = m_current_block;
        rv.path.insert(rv.path.begin(), m_current_composite.begin(), m_current_composite.end());
        rv.path.push_back(key_name);

        rv.value.m_type = TomlValue::Type::List;
        while( (t = Token::lex_from(m_if)).m_type != Token::Type::SquareClose )
        {
            // TODO: Recurse parse a value
            switch(t.m_type)
            {
            case Token::Type::String:
                rv.value.m_sub_values.push_back(TomlValue { t.as_string() });
                break;
            default:
                throw "";
            }

            t = Token::lex_from(m_if);
            if(t.m_type != Token::Type::Comma)
                break;
        }
        if(t.m_type != Token::Type::SquareClose)
            throw "";
        break;
    case Token::Type::BraceOpen:
        m_current_composite.push_back(key_name);
        DEBUG("Enter composite block " << m_current_block << ", " << m_current_composite);
        // Recurse to restart parse
        return get_next_value();
    case Token::Type::Ident:
        if( t.m_data == "true" )
        {
            rv.path = m_current_block;
            rv.path.insert(rv.path.begin(), m_current_composite.begin(), m_current_composite.end());
            rv.path.push_back(key_name);
            rv.value = TomlValue { true };
        }
        else if( t.m_data == "false" )
        {
            rv.path = m_current_block;
            rv.path.insert(rv.path.begin(), m_current_composite.begin(), m_current_composite.end());
            rv.path.push_back(key_name);

            rv.value = TomlValue { false };
        }
        else
        {
            throw "";
        }
        break;
    default:
        throw "";
    }

    t = Token::lex_from(m_if);
    while (!m_current_composite.empty() && t.m_type == Token::Type::BraceClose)
    {
        DEBUG("Leave composite block " << m_current_block << ", " << m_current_composite);
        m_current_composite.pop_back();
        t = Token::lex_from(m_if);
    }
    if( m_current_composite.empty() )
    {
        // TODO: Allow EOF?
        if(t.m_type != Token::Type::Newline)
            throw "";
    }
    else
    {
        if( t.m_type != Token::Type::Comma )
            throw "";
    }
    return rv;
}

Token Token::lex_from(::std::ifstream& is)
{
    auto rv = Token::lex_from_inner(is);
    DEBUG("lex_from: " << rv);
    return rv;
}
Token Token::lex_from_inner(::std::ifstream& is)
{
    int c;
    do
    {
        c = is.get();
    } while( c != EOF && c != '\n' && isspace(c) );

    ::std::string   str;
    switch(c)
    {
    case EOF:   return Token { Type::Eof };
    case '[':   return Token { Type::SquareOpen };
    case ']':   return Token { Type::SquareClose };
    case '{':   return Token { Type::BraceOpen };
    case '}':   return Token { Type::BraceClose };
    case ',':   return Token { Type::Comma };
    case '.':   return Token { Type::Dot };
    case '=':   return Token { Type::Assign };
    case '\n':  return Token { Type::Newline };
    case '#':
        while(c != '\n')
        {
            c = is.get();
            if(c == EOF)
                return Token { Type::Eof };
        }
        return Token { Type::Newline };
    case '\'':
        c = is.get();
        while (c != '\'')
        {
            if (c == EOF)
                throw "";
            if (c == '\\')
            {
                // TODO: Escaped strings
                throw "";
            }
            str += (char)c;
            c = is.get();
        }
        return Token { Type::String, str };
    case '"':
        c = is.get();
        while(c != '"')
        {
            if (c == EOF)
                throw "";
            if (c == '\\')
            {
                // TODO: Escaped strings
                throw "";
            }
            str += (char)c;
            c = is.get();
        }
        return Token { Type::String, str };
    default:
        if(isalpha(c))
        {
            // Identifier
            while(isalnum(c) || c == '-')
            {
                str += (char)c;
                c = is.get();
            }
            is.putback(c);
            return Token { Type::Ident, str };
        }
        else
        {
            throw "";
        }
    }
}