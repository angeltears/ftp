#include "common.h"
#include "str.h"


/**
 * str_trim_crlf -去除掉句尾/r/n /n
 * @str: 待处理的字符串
 */
void str_trim_crlf(char *str)
{
    char *p = &str[strlen(str) - 1];
	while (*p == '\r' || *p == '\n')
		*p-- = '\0';
}

/**
 * str_split -分割命令 将命令分割成cmd和参数
 * @str:  输入命令
 * @left: 输出cmd
 * @right: 输出cmdarg
 * @c: 分割标志
 */
void str_split(const char *str, char *left, char *right, char c)
{
        printf("11");
    char *p = strchr((char *)str, c);
    if (p == NULL)
    {
        strcpy(left, str);
    }
    else
    {
        strncpy(left, str, p - str);
        strcpy(right, p + 1);
    }
    printf("11");
}
int str_all_space(const char *str)
{
    return 0;
}
void str_upper(char *str)
{

}
long long str_to_longlong(const char *str)
{

}
unsigned int str_octal_to_uint(const char *str)
{

}