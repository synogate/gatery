/*
This is a minimal gdb stub for loading and executing.
launch gdb with the elf as argument and enter commands:

target remote 127.0.0.1:1337
target remote \\.\COM12
load
restore [filename] binary <offset> <start> <end>
c

example: riscv64-unknown-elf-gdb .\demo.elf -ex "target remote \\.\COM12" -ex "load"

list of gdbserv commands:           https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_129.html
enable debug output:                set debug remote 1
compile with:                       riscv64-unknown-elf-gcc -nostdlib -nodefaultlibs -fno-exceptions -nostartfiles -Os -fpic -march=rv32i -mabi=ilp32 .\ministub.c
placing on a fixed address did not change code size that much.
in my test compile the first 0x74 bytes (elf header) can be discarded (pic).
or use the following to extract text section:       riscv64-unknown-elf-objcopy -O binary -j .text .\a.out a.raw

*/

// implement the following to communicate to gdb
char stub_get();
void stub_msg_put(char c);


static void stub_msg_puts(const char* msg)
{
    while (*msg)
        stub_msg_put(*msg++);
}

static char num2hex(char c)
{
    //c &= 0xF;
    return ((c < 10) ? '0' : 'a' - 10) + c;
}

static char hex2num(char c)
{
    return c - (c >= '0' && c <= '9' ? '0' : 'a' - 10);
}

static void stub_msg_end(unsigned char sum)
{
    stub_msg_put('#');
    stub_msg_put(num2hex(sum >> 4));
    stub_msg_put(num2hex(sum & 0xF));

    while (1)
    {
        char c = stub_get();

        if (c == '+')
            break;
        if (c == '-')
            break;
    }
}

static unsigned stub_get_hexnumber()
{
    unsigned ret = 0;

    while (1)
    {
        char c = stub_get();
        unsigned char n = hex2num(c);
        if (n > 15)
            break;
        ret = (ret << 4) + n;
    }
    return ret;
}

static int access_memory(int write)
{
    volatile unsigned char* address = (volatile unsigned char*)stub_get_hexnumber();
    unsigned length = stub_get_hexnumber();
    int checksum = 0;

    while (length--)
    {
        if (write)
        {
            char c = stub_get();
            if (c == 0x7d)
            {
                c = stub_get();
                c ^= 0x20;
            }
            *address = c;
        }
        else
        {
            unsigned char c = *address;
            char ch = num2hex(c >> 4);
            stub_msg_put(ch); checksum += ch;

            ch = num2hex(c & 0xF);
            stub_msg_put(ch); checksum += ch;
        }

        address++;
    }
    return checksum;
}

typedef void (*t_entry_point)();

static void code_address(volatile t_entry_point* entry)
{
    unsigned reg = stub_get_hexnumber();
    unsigned val = stub_get_hexnumber();

    val = (val >> 24) |
        ((val & 0x00FF0000) >> 8) |
        ((val & 0x0000FF00) << 8) |
        (val << 24);

    if (reg == 32)
        *entry = (void*)val;
}

static int stub_enter(volatile t_entry_point* entry)
{
    while (stub_get() != '$');

    char cmd = stub_get();
    unsigned char checksum = 0;
    stub_msg_put('+');

    switch (cmd)
    {
    case 'c':
        (**entry)();
        // fall through
    case '?':
        stub_msg_put('$');
        stub_msg_put('S');
        stub_msg_put('0');
        stub_msg_put('5');
        checksum = 'S' + '0' + '5';
        break;
    case 'g':
        stub_msg_put('$');
        for (int i = 0; i < 8 * 33; ++i)
            stub_msg_put('0');
        checksum = (unsigned char)(8 * 33 * '0');
        break;
    case 'X':
        stub_msg_put('$');
        access_memory(1);
        break;
    case 'm':
        stub_msg_put('$');
        checksum = access_memory(0);
        break;
    case 'P':
        stub_msg_put('$');
        code_address(entry);
        break;

    default:
        stub_msg_put('$');
    }

    if (cmd == 'X' || cmd == 'P')
    {
        stub_msg_put('O');
        stub_msg_put('K');
        checksum = 'O' + 'K';
    }

    stub_msg_end(checksum);
    return 0;
}
