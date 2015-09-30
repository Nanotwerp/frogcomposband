#include "angband.h"

#include "z-doc.h"

#include <assert.h>

/* The Message Queue */
static int       _msg_max = 2048;
static int       _msg_count = 0;
static msg_ptr  *_msgs = NULL;
static int       _msg_head = 0;
static bool      _msg_append = FALSE;

/* The Message "Line" */
static rect_t    _msg_line_rect;
static doc_ptr   _msg_line_doc = NULL;
static doc_pos_t _msg_line_sync_pos;

msg_ptr _msg_alloc(cptr s)
{
    msg_ptr m = malloc(sizeof(msg_t));
    m->msg = string_alloc(s);
    m->turn = 0;
    m->count = 1;
    m->color = TERM_WHITE;
    return m;
}

void _msg_free(msg_ptr m)
{
    if (m)
    {
        string_free(m->msg);
        free(m);
    }
}

void msg_on_startup(void)
{
    int cb = _msg_max * sizeof(msg_ptr);
    _msgs = malloc(cb);
    memset(_msgs, 0, cb);
    _msg_count = 0;
    _msg_head = 0;
    _msg_append = FALSE;

    msg_line_init(NULL);
}

void msg_on_shutdown(void)
{
    int i;
    for (i = 0; i < _msg_max; i++)
        _msg_free(_msgs[i]);
    free(_msgs);
}

static int _msg_index(int age)
{
    assert(0 <= age && age < _msg_max);
    return (_msg_head + _msg_max - (age + 1)) % _msg_max;
}

int msg_count(void)
{
    return _msg_count;
}

msg_ptr msg_get(int age)
{
    int i = _msg_index(age);
    return _msgs[i];
}

int msg_get_plain_text(int age, char *buffer, int max)
{
    int         ct = 0, i;
    doc_token_t token;
    msg_ptr     msg = msg_get(age);
    cptr        pos = string_buffer(msg->msg);
    bool        done = FALSE;

    while (!done)
    {
        pos = doc_lex(pos, &token);
        if (token.type == DOC_TOKEN_EOF) break;
        if (token.type == DOC_TOKEN_TAG) continue; /* assume only color tags */
        for (i = 0; i < token.size; i++)
        {
            if (ct >= max - 4)
            {
                buffer[ct++] = '.';
                buffer[ct++] = '.';
                buffer[ct++] = '.';
                done = TRUE;
                break;
            }
            buffer[ct++] = token.pos[i];
        }
    }
    buffer[ct] = '\0';
    return ct;
}

void msg_add(cptr str)
{
    cmsg_add(TERM_WHITE, str);
}

void cmsg_append(byte color, cptr str)
{
    if (!_msg_count)
        cmsg_add(color, str);
    else
    {
        msg_ptr m = msg_get(0);

        assert(m);
        if (string_length(m->msg))
            string_append_char(m->msg, ' ');
        if (color != m->color)
            string_printf(m->msg, "<color:%c>%s<color:*>", attr_to_attr_char(color), str);
        else
            string_append(m->msg, str);
    }
}

void _cmsg_add_aux(byte color, cptr str, int turn, int count)
{
    msg_ptr m;

    /* Repeat last message? */
    if (_msg_count)
    {
        m = msg_get(0);
        if (strcmp(string_buffer(m->msg), str) == 0)
        {
            m->count += count;
            m->turn = turn;
            m->color = color;
            return;
        }
    }

    m = _msgs[_msg_head];
    if (!m)
    {
        m = _msg_alloc(NULL);
        _msgs[_msg_head] = m;
        _msg_count++;
    }
    else
    {
        string_clear(m->msg);
        string_shrink(m->msg, 128);
    }
    string_append(m->msg, str);

    m->turn = turn;
    m->count = count;
    m->color = color;

    _msg_head = (_msg_head + 1) % _msg_max;
}

void cmsg_add(byte color, cptr str)
{
    _cmsg_add_aux(color, str, player_turn, 1);
}

rect_t msg_line_rect(void)
{
    assert(_msg_line_doc); /* call msg_on_startup()! */
    return rect_create(
        _msg_line_rect.x,
        _msg_line_rect.y,
        _msg_line_rect.cx,
        doc_line_count(_msg_line_doc)
    );
}

void msg_line_init(const rect_t *display_rect)
{
    if (display_rect && rect_is_valid(display_rect))
    {
        if (_msg_line_doc)
        {
            msg_line_clear();
            doc_free(_msg_line_doc);
        }
        _msg_line_rect = *display_rect;
        if (_msg_line_rect.x + _msg_line_rect.cx > Term->wid)
            _msg_line_rect.cx = Term->wid - _msg_line_rect.x;
        _msg_line_doc = doc_alloc(_msg_line_rect.cx);
        _msg_line_sync_pos = doc_cursor(_msg_line_doc);
    }
    else
    {
        rect_t r = rect_create(13, 0, 72, 10);
        msg_line_init(&r);
    }
}

void msg_boundary(void)
{
    _msg_append = FALSE;
    if (auto_more_state == AUTO_MORE_SKIP_BLOCK)
    {
        /* Flush before updating the state to skip
           the remnants of the last message from the
           previous message block */
        msg_print(NULL);
        auto_more_state = AUTO_MORE_PROMPT;
    }
}

bool msg_line_contains(int row, int col)
{
    rect_t r = msg_line_rect();
    if (col < 0)
        col = r.x;
    if (row < 0)
        row = r.y;
    return rect_contains_pt(&r, col, row);
}

bool msg_line_is_empty(void)
{
    doc_pos_t cursor = doc_cursor(_msg_line_doc);
    if (cursor.x == 0 && cursor.y == 0)
        return TRUE;
    return FALSE;
}

void msg_line_clear(void)
{
    int i;
    int y = doc_cursor(_msg_line_doc).y;

    for (i = 0; i <= y; i++)
        Term_erase(_msg_line_rect.x, _msg_line_rect.y + i, _msg_line_rect.cx);

    doc_rollback(_msg_line_doc, doc_pos_create(0, 0));
    _msg_line_sync_pos = doc_cursor(_msg_line_doc);

    if (y > 0)
    {
        /* Note: We need not redraw the entire map if this proves too slow */
        p_ptr->redraw |= PR_MAP;
        if (_msg_line_rect.x <= 12)
            p_ptr->redraw |= PR_BASIC | PR_EQUIPPY;
    }
}

void msg_line_redraw(void)
{
    doc_sync_term(
        _msg_line_doc,
        doc_range_all(_msg_line_doc),
        doc_pos_create(_msg_line_rect.x, _msg_line_rect.y)
    );
    _msg_line_sync_pos = doc_cursor(_msg_line_doc);
}

static void msg_line_sync(void)
{
    doc_sync_term(
        _msg_line_doc,
        doc_range_bottom(_msg_line_doc, _msg_line_sync_pos),
        doc_pos_create(_msg_line_rect.x, _msg_line_rect.y + _msg_line_sync_pos.y)
    );
    _msg_line_sync_pos = doc_cursor(_msg_line_doc);
/*  inkey(); */
}

static void msg_line_flush(void)
{
    if (auto_more_state == AUTO_MORE_PROMPT)
    {
        doc_insert_text(_msg_line_doc, TERM_L_BLUE, "-more-");
        msg_line_sync();

        for(;;)
        {
            int cmd = inkey();
            if (cmd == ESCAPE)
            {
                auto_more_state = AUTO_MORE_SKIP_ALL;
                break;
            }
            else if (cmd == '\n' || cmd == '\r' || cmd == 'n')
            {
                auto_more_state = AUTO_MORE_SKIP_BLOCK;
                break;
            }
            else if (cmd == '?')
            {
                screen_save_aux();
                show_file(TRUE, "context_more_prompt.txt", NULL, 0, 0);
                screen_load_aux();
                continue;
            }
            else if (cmd == ' ' || cmd == 'm' || quick_messages)
            {
                break;
            }
            bell();
        }
    }
    msg_line_clear();
}


static void msg_line_display(byte color, cptr msg)
{
    /* Quick and dirty test for -more- ... This means _msg_line_display_rect
       is just a suggested limit, and we'll surpass this for long messages. */
    if (doc_cursor(_msg_line_doc).y >= _msg_line_rect.cy)
        msg_line_flush();

    /* Append this message to the last? */
    else if (!_msg_append && !msg_line_is_empty() && doc_cursor(_msg_line_doc).x > 0)
        doc_newline(_msg_line_doc);

    doc_insert_text(_msg_line_doc, color, msg);
    if (doc_cursor(_msg_line_doc).x > 0)
        doc_insert_char(_msg_line_doc, ' ');
    msg_line_sync();
}

/* Prompt the user for a choice:
   [1] keys[0] is the default choice if quick_messages and PROMPT_FORCE_CHOICE is not set.
   [2] keys[0] is returned on ESC if PROMPT_ESCAPE_DEFAULT is set.
   [3] You get back the char in the keys prompt, not the actual character pressed.
       This makes a difference if PROMPT_CASE_SENSITIVE is not set (and simplifies
       your coding).

   Sample Usage:
   char ch = cmsg_prompt(TERM_VIOLET, "Really commit suicide? [Y,n]", "nY", PROMPT_NEW_LINE | PROMPT_CASE_SENSITIVE);
   if (ch == 'Y') {...}
*/
static char cmsg_prompt_imp(byte color, cptr prompt, char keys[], int options)
{
    if (options & PROMPT_NEW_LINE)
        msg_boundary();

    auto_more_state = AUTO_MORE_PROMPT;
    cmsg_print(color, prompt);

    for (;;)
    {
        char ch = inkey();
        int  i;

        if (ch == ESCAPE && (options & PROMPT_ESCAPE_DEFAULT))
            return keys[0];

        for (i = 0; ; i++)
        {
            char choice = keys[i];
            if (!choice) break;
            if (ch == choice) return choice;
            if (!(options & PROMPT_CASE_SENSITIVE))
            {
                if (tolower(ch) == tolower(choice)) return choice;
            }
        }

        if (!(options & PROMPT_FORCE_CHOICE) && quick_messages)
            return keys[0];
    }
}

char cmsg_prompt(byte color, cptr prompt, char keys[], int options)
{
    char ch = cmsg_prompt_imp(color, prompt, keys, options);
    msg_line_clear();
    return ch;
}

char msg_prompt(cptr prompt, char keys[], int options)
{
    return cmsg_prompt(TERM_WHITE, prompt, keys, options);
}

void cmsg_print(byte color, cptr msg)
{
    if (world_monster) return;
    if (statistics_hack) return;

    /* Hack: msg_print(NULL) requests a flush if needed */
    if (!msg)
    {
        if (!msg_line_is_empty())
            msg_line_flush();
        return;
    }

    if (character_generated)
    {
        if (_msg_append && _msg_count)
            cmsg_append(color, msg);
        else
            cmsg_add(color, msg);
    }

    msg_line_display(color, msg);

    if (auto_more_state == AUTO_MORE_SKIP_ONE)
        auto_more_state = AUTO_MORE_PROMPT;

    p_ptr->window |= PW_MESSAGE;
    window_stuff();
    if (fresh_message) /* ?? */
        Term_fresh();

    _msg_append = TRUE;
}

void msg_print(cptr msg)
{
    cmsg_print(TERM_WHITE, msg);
}

void msg_format(cptr fmt, ...)
{
    va_list vp;
    char buf[1024];

    va_start(vp, fmt);
    (void)vstrnfmt(buf, 1024, fmt, vp);
    va_end(vp);

    cmsg_print(TERM_WHITE, buf);
}

void cmsg_format(byte color, cptr fmt, ...)
{
    va_list vp;
    char buf[1024];

    va_start(vp, fmt);
    (void)vstrnfmt(buf, 1024, fmt, vp);
    va_end(vp);

    cmsg_print(color, buf);
}

void msg_on_load(savefile_ptr file)
{
    int i;
    char buf[1024];
    int count = savefile_read_s16b(file);

    for (i = 0; i < count; i++)
    {
        s32b turn = 0;
        s32b count = 0;
        byte color = TERM_WHITE;

        savefile_read_string(file, buf, sizeof(buf));
        turn = savefile_read_s32b(file);
        count = savefile_read_s32b(file);
        color = savefile_read_byte(file);

        _cmsg_add_aux(color, buf, turn, count);
    }
}

void msg_on_save(savefile_ptr file)
{
    int i;
    int count = msg_count();
    if (compress_savefile && count > 40) count = 40;

    savefile_write_u16b(file, count);
    for (i = count - 1; i >= 0; i--)
    {
        msg_ptr m = msg_get(i);
        savefile_write_string(file, string_buffer(m->msg));
        savefile_write_s32b(file, m->turn);
        savefile_write_s32b(file, m->count);
        savefile_write_byte(file, m->color);
    }
}


