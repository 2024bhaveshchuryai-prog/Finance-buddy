/* finance_buddy.c
   Finance Buddy - CLI finance manager demonstrating data structures in C.
   Compile: gcc -o finance_buddy finance_buddy.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------
   Data structure definitions
   ------------------------------*/
typedef struct Transaction {
    int id;
    char type[16]; // "DEPOSIT", "WITHDRAW", "TRANSFER"
    double amount;
    int to_account; // for transfer: destination account id (0 if N/A)
    char timestamp[64];
    struct Transaction *next;
} Transaction;

typedef struct Account {
    int id;
    char name[64];
    double balance;
    Transaction *tx_head; // linked list of transactions (newest at head)
    struct Account *next; // linked list of accounts
} Account;

/* Stack node for undo */
typedef struct OpNode {
    char op_type[16]; // "DEPOSIT","WITHDRAW","TRANSFER","CREATE"
    int acc_id;
    int acc_id_to; // transfer destination
    double amount;
    struct OpNode *next;
} OpNode;

/* ------------------------------
   Global heads
   ------------------------------*/
Account *accounts_head = NULL;
OpNode *undo_stack = NULL;
int next_account_id = 1;
int next_tx_id = 1;

/* ------------------------------
   Utility functions
   ------------------------------*/
char *current_time_str(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

Account* find_account(int id) {
    Account *cur = accounts_head;
    while (cur) {
        if (cur->id == id) return cur;
        cur = cur->next;
    }
    return NULL;
}

void push_undo(const char *op, int acc_id, int acc_id_to, double amount) {
    OpNode *n = malloc(sizeof(OpNode));
    strcpy(n->op_type, op);
    n->acc_id = acc_id;
    n->acc_id_to = acc_id_to;
    n->amount = amount;
    n->next = undo_stack;
    undo_stack = n;
}

OpNode* pop_undo() {
    if (!undo_stack) return NULL;
    OpNode *top = undo_stack;
    undo_stack = undo_stack->next;
    return top;
}

/* ------------------------------
   Transaction helpers
   ------------------------------*/
Transaction* create_transaction(const char *type, double amount, int to_account) {
    Transaction *t = malloc(sizeof(Transaction));
    t->id = next_tx_id++;
    strncpy(t->type, type, sizeof(t->type)-1);
    t->amount = amount;
    t->to_account = to_account;
    current_time_str(t->timestamp, sizeof(t->timestamp));
    t->next = NULL;
    return t;
}

void add_transaction(Account *acc, Transaction *tx) {
    // insert at head for newest-first order
    tx->next = acc->tx_head;
    acc->tx_head = tx;
}

/* ------------------------------
   Core operations
   ------------------------------*/
Account* create_account(const char *name, double opening_balance) {
    Account *acc = malloc(sizeof(Account));
    acc->id = next_account_id++;
    strncpy(acc->name, name, sizeof(acc->name)-1);
    acc->balance = opening_balance;
    acc->tx_head = NULL;
    acc->next = accounts_head;
    accounts_head = acc;

    // record opening as a deposit transaction for trace
    Transaction *tx = create_transaction("DEPOSIT", opening_balance, 0);
    add_transaction(acc, tx);

    push_undo("CREATE", acc->id, 0, opening_balance);
    return acc;
}

int deposit(int acc_id, double amount) {
    Account *acc = find_account(acc_id);
    if (!acc) return 0;
    acc->balance += amount;
    Transaction *tx = create_transaction("DEPOSIT", amount, 0);
    add_transaction(acc, tx);
    push_undo("DEPOSIT", acc_id, 0, amount);
    return 1;
}

int withdraw(int acc_id, double amount) {
    Account *acc = find_account(acc_id);
    if (!acc) return 0;
    if (acc->balance < amount) return -1; // insufficient funds
    acc->balance -= amount;
    Transaction *tx = create_transaction("WITHDRAW", amount, 0);
    add_transaction(acc, tx);
    push_undo("WITHDRAW", acc_id, 0, amount);
    return 1;
}

int transfer_funds(int from_id, int to_id, double amount) {
    if (from_id == to_id) return -2;
    Account *from = find_account(from_id);
    Account *to = find_account(to_id);
    if (!from || !to) return 0;
    if (from->balance < amount) return -1;
    from->balance -= amount;
    to->balance += amount;
    Transaction *tx_from = create_transaction("TRANSFER", amount, to_id);
    Transaction *tx_to = create_transaction("TRANSFER", amount, from_id);
    add_transaction(from, tx_from);
    add_transaction(to, tx_to);
    push_undo("TRANSFER", from_id, to_id, amount);
    return 1;
}

/* Undo last operation */
void undo_last() {
    OpNode *op = pop_undo();
    if (!op) {
        printf("Nothing to undo.\n");
        return;
    }
    if (strcmp(op->op_type, "DEPOSIT") == 0) {
        Account *acc = find_account(op->acc_id);
        if (acc) {
            if (acc->balance >= op->amount) {
                acc->balance -= op->amount;
                Transaction *tx = create_transaction("UNDO_DEPOSIT", op->amount, 0);
                add_transaction(acc, tx);
                printf("Undid deposit of %.2f from account %d\n", op->amount, op->acc_id);
            } else {
                printf("Cannot undo deposit: insufficient balance in account %d\n", op->acc_id);
            }
        }
    } else if (strcmp(op->op_type, "WITHDRAW") == 0) {
        Account *acc = find_account(op->acc_id);
        if (acc) {
            acc->balance += op->amount;
            Transaction *tx = create_transaction("UNDO_WITHDRAW", op->amount, 0);
            add_transaction(acc, tx);
            printf("Undid withdraw of %.2f to account %d\n", op->amount, op->acc_id);
        }
    } else if (strcmp(op->op_type, "TRANSFER") == 0) {
        Account *from = find_account(op->acc_id);
        Account *to = find_account(op->acc_id_to);
        if (from && to && to->balance >= op->amount) {
            from->balance += op->amount;
            to->balance -= op->amount;
            Transaction *txFrom = create_transaction("UNDO_TRANSFER", op->amount, op->acc_id_to);
            Transaction *txTo = create_transaction("UNDO_TRANSFER", op->amount, op->acc_id);
            add_transaction(from, txFrom);
            add_transaction(to, txTo);
            printf("Undid transfer of %.2f from %d to %d\n", op->amount, op->acc_id, op->acc_id_to);
        } else {
            printf("Cannot undo transfer automatically (balances mismatch or accounts missing).\n");
        }
    } else if (strcmp(op->op_type, "CREATE") == 0) {
        // delete account created (simple removal from linked list) if present and zero or only opening balance
        Account *prev = NULL, *cur = accounts_head;
        while (cur) {
            if (cur->id == op->acc_id) break;
            prev = cur;
            cur = cur->next;
        }
        if (cur) {
            // Only remove if balance equals opening amount and there are no other txs? We'll remove anyway but warn.
            if (prev) prev->next = cur->next;
            else accounts_head = cur->next;
            // free txs
            Transaction *t = cur->tx_head;
            while (t) {
                Transaction *tmp = t;
                t = t->next;
                free(tmp);
            }
            free(cur);
            printf("Undid creation of account %d\n", op->acc_id);
        } else {
            printf("Account to undo creation not found.\n");
        }
    } else {
        printf("Unknown undo operation: %s\n", op->op_type);
    }
    free(op);
}

/* ------------------------------
   Persistence (save/load)
   Simple flat format:
   Accounts:
   ACC|id|name|balance
   TX|acc_id|tx_id|type|amount|to_acc|timestamp
   ------------------------------*/
void save_data(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error opening file to save");
        return;
    }
    Account *a = accounts_head;
    while (a) {
        fprintf(f, "ACC|%d|%s|%.2f\n", a->id, a->name, a->balance);
        Transaction *t = a->tx_head;
        while (t) {
            // replace '|' in timestamp or type if any (not expected)
            fprintf(f, "TX|%d|%d|%s|%.2f|%d|%s\n", a->id, t->id, t->type, t->amount, t->to_account, t->timestamp);
            t = t->next;
        }
        a = a->next;
    }
    fclose(f);
    printf("Data saved to %s\n", filename);
}

void free_all_data() {
    Account *a = accounts_head;
    while (a) {
        Transaction *t = a->tx_head;
        while (t) {
            Transaction *tmp = t;
            t = t->next;
            free(tmp);
        }
        Account *atmp = a;
        a = a->next;
        free(atmp);
    }
    accounts_head = NULL;
}

void load_data(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        // file may not exist => not an error
        return;
    }
    free_all_data();
    char line[512];
    int max_acc_id = 0;
    int max_tx_id = 0;
    while (fgets(line, sizeof(line), f)) {
        // strip newline
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        if (strncmp(line, "ACC|", 4) == 0) {
            int id;
            char name[128];
            double balance;
            sscanf(line+4, "%d|%127[^|]|%lf", &id, name, &balance);
            Account *acc = malloc(sizeof(Account));
            acc->id = id;
            strncpy(acc->name, name, sizeof(acc->name)-1);
            acc->balance = balance;
            acc->tx_head = NULL;
            acc->next = accounts_head;
            accounts_head = acc;
            if (id > max_acc_id) max_acc_id = id;
        } else if (strncmp(line, "TX|", 3) == 0) {
            int acc_id, txid, toacc;
            char type[64], ts[64];
            double amt;
            // TX|acc_id|tx_id|type|amount|to_acc|timestamp
            // we need to parse carefully for timestamp which may contain spaces
            char *p = line + 3;
            char *parts[7]; int pi = 0;
            parts[pi++] = p;
            while (*p && pi < 7) {
                if (*p == '|') {
                    *p = '\0';
                    parts[pi++] = p+1;
                }
                p++;
            }
            // now parts[]: [acc_id, tx_id, type, amount, to_acc, timestamp]
            if (pi >= 6) {
                acc_id = atoi(parts[0]);
                txid = atoi(parts[1]);
                strncpy(type, parts[2], sizeof(type)-1);
                amt = atof(parts[3]);
                toacc = atoi(parts[4]);
                strncpy(ts, parts[5], sizeof(ts)-1);
                Account *acc = find_account(acc_id);
                if (acc) {
                    Transaction *t = malloc(sizeof(Transaction));
                    t->id = txid;
                    strncpy(t->type, type, sizeof(t->type)-1);
                    t->amount = amt;
                    t->to_account = toacc;
                    strncpy(t->timestamp, ts, sizeof(t->timestamp)-1);
                    t->next = acc->tx_head;
                    acc->tx_head = t;
                    if (txid > max_tx_id) max_tx_id = txid;
                }
            }
        }
    }
    fclose(f);
    next_account_id = max_acc_id + 1;
    next_tx_id = max_tx_id + 1;
}

/* ------------------------------
   UI helpers
   ------------------------------*/
void list_accounts() {
    printf("Accounts:\n");
    Account *a = accounts_head;
    if (!a) { printf("  (no accounts yet)\n"); return; }
    while (a) {
        printf("  ID:%d  Name:%s  Balance:%.2f\n", a->id, a->name, a->balance);
        a = a->next;
    }
}

void show_account_transactions(int acc_id) {
    Account *a = find_account(acc_id);
    if (!a) { printf("Account not found.\n"); return; }
    printf("Transactions for %s (ID %d) [newest first]:\n", a->name, a->id);
    Transaction *t = a->tx_head;
    if (!t) { printf("  (no transactions)\n"); return; }
    while (t) {
        if (strcmp(t->type, "TRANSFER") == 0) {
            printf("  [%s] %s %.2f  to/from acc %d  (%s)\n", t->timestamp, t->type, t->amount, t->to_account, t->type);
        } else {
            printf("  [%s] %s %.2f\n", t->timestamp, t->type, t->amount);
        }
        t = t->next;
    }
}

/* ------------------------------
   Main menu
   ------------------------------*/
void print_menu() {
    puts("\n--- Finance Buddy ---");
    puts("1) Create account");
    puts("2) List accounts");
    puts("3) Deposit");
    puts("4) Withdraw");
    puts("5) Transfer");
    puts("6) View transactions");
    puts("7) Undo last operation");
    puts("8) Save data");
    puts("9) Load data");
    puts("0) Exit");
    printf("Choose: ");
}

int main() {
    const char *datafile = "finance_data.txt";
    load_data(datafile);
    printf("Welcome to Finance Buddy (Data file: %s)\n", datafile);

    while (1) {
        print_menu();
        int choice;
        if (scanf("%d", &choice) != 1) { // flush
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
            continue;
        }
        if (choice == 0) {
            save_data(datafile);
            printf("Exiting. Data saved.\n");
            break;
        }
        if (choice == 1) {
            char name[64];
            double ob;
            printf("Enter account holder name: ");
            while (getchar() != '\n');
            fgets(name, sizeof(name), stdin);
            char *nl = strchr(name, '\n'); if (nl) *nl = '\0';
            printf("Enter opening balance: ");
            scanf("%lf", &ob);
            Account *acc = create_account(name, ob);
            printf("Created account %s with ID %d\n", acc->name, acc->id);
        } else if (choice == 2) {
            list_accounts();
        } else if (choice == 3) {
            int id; double amt;
            printf("Account ID: "); scanf("%d", &id);
            printf("Amount to deposit: "); scanf("%lf", &amt);
            int r = deposit(id, amt);
            if (r) printf("Deposited %.2f to account %d\n", amt, id);
            else printf("Account not found.\n");
        } else if (choice == 4) {
            int id; double amt;
            printf("Account ID: "); scanf("%d", &id);
            printf("Amount to withdraw: "); scanf("%lf", &amt);
            int r = withdraw(id, amt);
            if (r == 1) printf("Withdrawn %.2f from account %d\n", amt, id);
            else if (r == -1) printf("Insufficient funds.\n");
            else printf("Account not found.\n");
        } else if (choice == 5) {
            int from, to; double amt;
            printf("From account ID: "); scanf("%d", &from);
            printf("To account ID: "); scanf("%d", &to);
            printf("Amount to transfer: "); scanf("%lf", &amt);
            int r = transfer_funds(from, to, amt);
            if (r == 1) printf("Transferred %.2f from %d to %d\n", amt, from, to);
            else if (r == -1) printf("Insufficient funds.\n");
            else if (r == 0) printf("One of accounts not found.\n");
            else if (r == -2) printf("Source and destination cannot be same.\n");
        } else if (choice == 6) {
            int id; printf("Account ID: "); scanf("%d", &id);
            show_account_transactions(id);
        } else if (choice == 7) {
            undo_last();
        } else if (choice == 8) {
            save_data(datafile);
        } else if (choice == 9) {
            load_data(datafile);
            printf("Data loaded.\n");
        } else {
            printf("Invalid choice.\n");
        }
    }

    // free memory
    free_all_data();
    while (undo_stack) {
        OpNode *tmp = undo_stack;
        undo_stack = undo_stack->next;
        free(tmp);
    }
    return 0;
}
