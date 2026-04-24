#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <ctype.h>

// ================= MEMORY =================
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// ================= FETCH =================
char* fetch_url(const char *url) {
    CURL *curl;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        printf("Erro curl\n");
        return NULL;
    }

    curl_easy_cleanup(curl);
    return chunk.memory;
}

// ================= HTML → TEXT =================
char* extract_text(const char *html) {
    htmlDocPtr doc = htmlReadMemory(html, strlen(html), NULL, NULL,
        HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

    if (!doc) return NULL;

    xmlChar *txt = xmlNodeGetContent(xmlDocGetRootElement(doc));

    char *result = strdup((char*)txt);

    xmlFree(txt);
    xmlFreeDoc(doc);

    return result;
}

// ================= COUNT =================
int count_substring(const char *text, const char *sub) {
    int count = 0;
    const char *p = text;

    while ((p = strstr(p, sub))) {
        count++;
        p++;
    }
    return count;
}

// ================= MAIN =================
int main() {

    printf("VOYNICH EXACORDEX (TERMUX)\n");

    char *html = fetch_url("https://en.wikipedia.org/wiki/Voynich_manuscript");

    if (!html) {
        printf("Falha na web\n");
        return 1;
    }

    char *text = extract_text(html);

    if (!text) {
        printf("Falha parse\n");
        return 1;
    }

    printf("\n--- ANALISE ---\n");

    printf("123: %d\n", count_substring(text, "123"));
    printf("0123: %d\n", count_substring(text, "0123"));
    printf("01123: %d\n", count_substring(text, "01123"));
    printf("0001123: %d\n", count_substring(text, "0001123"));

    printf("42: %d\n", count_substring(text, "42"));

    free(html);
    free(text);

    return 0;
}
