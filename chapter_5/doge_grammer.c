#include "mpc.h"

int main(int arg, char** argv) {

    mpc_parser_t* Adjective = mpc_new("adjective");
    mpc_parser_t* Noun      = mpc_new("noun");
    mpc_parser_t* Phrase    = mpc_new("phrase");
    mpc_parser_t* Doge      = mpc_new("doge");
    
    mpc_lang(MPC_LANG_DEFAULT,
        "                                           \
          adjective : \"wow\" | \"many\"            \
                    |  \"so\" | \"such\";           \
          noun      : \"lisp\" | \"language\"       \
                    | \"book\" | \"build\" | \"c\"; \
          phrase    : <adjective> <noun>;           \
          doge      : <phrase>*;                    \
        ",
        Adjective, Noun, Phrase, Doge);
     
     /* Do somke parsing here...*/
     mpc_cleanup(4, Adjective, Noun, Phrase, Doge);
     
     return 0;
     
}
