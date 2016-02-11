#include "tests.hpp"

new_test_suite(many_ed_signing);
new_test_suite(base_tests);
new_test_suite(bitwallet);
new_test_suite(wallet_io);

using std::thread;
using std::mutex;
using std::atomic;

atomic<size_t> tests_counter(0);
mutex mtx;


bool test_all(int number_of_threads) {

    ptest::config_t config_default;
    std::fstream file("./test.log", ios_base::out | ios_base::trunc);
    config_default.set_all_output_to(file);
    config_default.print_passed_tests = true;

    many_ed_signing.config = config_default;
    base_tests.config = config_default;
    //bitwallet.config = config_default;
    wallet_io.config = config_default;

    //ptest::general_suite.config = config_default;

    int test_loop = 1000, msg_length = 64;

    ptest::call_test(number_of_threads,
                            [&number_of_threads, &test_loop, &msg_length] () {
                                run_suite_test(many_ed_signing,test_manyEdSigning, number_of_threads, test_loop, msg_length, false, pequal);
                            }
                    );

    run_suite_test(base_tests,test_readableEd, 0, pequal);
    run_suite_test(base_tests,test_user_sending , 0, pequal);
    run_suite_test(base_tests,test_many_users , 0, pequal);
    run_suite_test(base_tests,test_cheater , 0, pequal);
    run_suite_test(base_tests,test_bad_chainsign, 0, pequal);
    run_suite_test(base_tests,test_convrt_tokenpacket, 0, pequal);
    run_suite_test(base_tests,test_netuser, 0, pequal);

    run_suite_test(wallet_io,test_wallet_expected_sender, 0, pequal);
    run_suite_test(wallet_io,test_wallet_mint_check, 0, pequal);
    run_suite_test(wallet_io,test_mint_token_expiration, 10, pequal);
    run_suite_test(wallet_io,test_user_recieve_deprecated, 0, pequal);

    //run_suite_test(bitwallet,test_rpcwallet, 0, pequal);

//    print_final_suite_result(many_ed_signing);
//    print_final_suite_result(base_tests);
//    print_final_suite_result(bitwallet);
    print_final_suite_result(wallet_io);

    print_final_test_result();
}

static std::string generate_random_string (size_t length) {
    auto generate_random_char = [] () -> char {
        static const char Charset[] = "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(Charset) - 1);
        return Charset[rand() % max_index];
    };
    std::string str(length, 0);
    generate_n(str.begin(), length, generate_random_char);
    return str;
}


bool test_readableEd () {
    std::cout << "RUNNING TEST READABLE_CRYPTO_ED" << std::endl;
    c_ed25519 edtest;
    std::string str_pubkey = edtest.get_public_key();
    unique_ptr<unsigned char[]> utab_pubkey = edtest.get_public_key_uC();
    std::cout << "ed: original utab pubkey = " << utab_pubkey.get() << std::endl;
    std::cout << "ed: string format pubkey = " << str_pubkey << std::endl;
    unique_ptr<unsigned char[]> utab_pubkey_afttrans (readable_toUchar(str_pubkey, pub_key_size));
    std::cout << "ed: back to short format = " << utab_pubkey_afttrans.get() << std::endl;

    const std::string message = "3fd30542fe3f61b24bd3a4b2dc0b6fb37fa6f63ebce52dd1778baa8c4dc02cff";
    std::string sign = edtest.sign(message);

    /* verify the signature */
    if (edtest.verify(sign, message, str_pubkey)) {
        std::cout << "valid signature\n" << std::endl;
    } else {
        std::cout << "invalid signature\n" << std::endl;
        return true;
    }

    return false;
}

bool test_manyEdSigning(int number_of_threads, size_t signs_num, size_t message_len) {
    c_ed25519 edtest;
    std::string str_pubkey = edtest.get_public_key();


    for(size_t i = 1; i < signs_num; ++i) {
        const std::string message = generate_random_string(message_len);
        std::string sign = edtest.sign(message);

        /* verify the signature */
        if (run_suite_assert (many_ed_signing, edtest.verify(sign, message, str_pubkey) == true, "invalid signature!") == pfailed) return true;

        mtx.lock();
        if( !(i % ((signs_num*number_of_threads)/100))) {
            tests_counter++;

        std::cout << "[" << tests_counter << "%]"
                  << "\b\b\b\b\b\b" << std::flush;
        }
        mtx.unlock();
    }
    return false;
}

bool test_user_sending () {
    std::cout << "RUNNING TEST USER_SENDING" << std::endl;
    c_user test_userA("userA");
    c_user test_userB("userB");

    test_userA.emit_tokens(1);
    test_userA.send_token_bymethod(test_userB);

    return false;
}

bool test_many_users () {
    std::cout << "RUNNING TEST02 MANY_USER" << std::endl;
    c_user A("userA"), B("userB"), C("userC"), D("userD");

    A.emit_tokens(1);
    A.send_token_bymethod(B);
    B.send_token_bymethod(C);
    C.send_token_bymethod(D);
    D.send_token_bymethod(A);
    return false;
}

bool test_cheater() {

    std::cout << "RUNNING TEST03 CHEATER" << std::endl;
    c_user A("userA"), B("userB"), C("userC"), X("userX");
    A.emit_tokens(1);


    A.send_token_bymethod(B);
    B.send_token_bymethod(C,1);
    B.send_token_bymethod(X);
    X.send_token_bymethod(B);
    B.send_token_bymethod(X);
    X.send_token_bymethod(B);
    B.send_token_bymethod(A);
    C.send_token_bymethod(X);
    X.send_token_bymethod(A); // should detect cheater

    A.emit_tokens(2);
    B.emit_tokens(1);
    B.send_token_bymethod(A);
    A.print_status(std::cout);

    return false;
}

bool test_bad_chainsign() {
    int errors = 3;
 try{
    c_token("$0|ed5c06c0577e0bf8a22e5f2c09f471e2c288bedfe255882d265661bf382e784232"
            "&ed1efe422eb7e56eb77d86efdac68b24da0ec4667ec44e45cd9db54389effb533"
            "a2a7f26496e630e8e0877eb96dae7a6d2ee2b6aaa84a0a755f645239597f25e02"
            "&userA&eda1da39fb7fe9b34013f26b20a6b6550c31ef4452c8f2602b5be19be289840204"
            "$12|ede83da978ac24294ad3c4ce1a14a184dcaf1ce2d35551242986efb7dd4cca68ea"
            "&ed61ccfae983e6660505443ef89e1749c1e9007920793c7c935737822d26c8e70"
            "ba433fa25bb19abf5e3d4fe57f466ea001711d369d4505a10d868278db86b200b"
            "&userB&ed5c06c0577e0bf8a22e5f2c09f471e2c288bedfe255882d265661bf382e784232"
            "$234|edb1fdafc96abac26c52dae3be8b68e3b8e479ebdcc3dbf97e81bf1f47d0472bcf"
            "&edbde6d2088a2a3520ff5e0222cbb18465e0e3f2b39ca4b45a7ae97b3e56e6909"
            "7783d0b2c54e2d9f12f7d7d5fdd4c80d936e1b7e7006cad4eaf731133a935c20a"
            "&userC&ede83da978ac24294ad3c4ce1a14a184dcaf1ce2d35551242986efb7dd4cca68ea"); // bad ids
 } catch(std::exception &message) {
    std::cout << message.what() << " -- OK" << std::endl;
    errors--;
 }
 try {
    c_token("$|ed5c06c0577e0bf8a22e5f2c09f471e2c288bedfe255882d265661bf382e784232"
            "&ed1efe422eb7e56eb77d86efdac68b24da0ec4667ec44e45cd9db54389effb533"
            "a2a7f26496e630e8e0877eb96dae7a6d2ee2b6aaa84a0a755f645239597f25e02"
            "&userA&eda1da39fb7fe9b34013f26b20a6b6550c31ef4452c8f2602b5be19be289840204"
            "$ede83da978ac24294ad3c4ce1a14a184dcaf1ce2d35551242986efb7dd4cca68ea"
            "&ed61ccfae983e6660505443ef89e1749c1e9007920793c7c935737822d26c8e70"
            "ba433fa25bb19abf5e3d4fe57f466ea001711d369d4505a10d868278db86b200b"
            "&userB&ed5c06c0577e0bf8a22e5f2c09f471e2c288bedfe255882d265661bf382e784232"
            "$0|edb1fdafc96abac26c52dae3be8b68e3b8e479ebdcc3dbf97e81bf1f47d0472bcf"
            "&edbde6d2088a2a3520ff5e0222cbb18465e0e3f2b39ca4b45a7ae97b3e56e6909"
            "7783d0b2c54e2d9f12f7d7d5fdd4c80d936e1b7e7006cad4eaf731133a935c20a"
            "&userC&ede83da978ac24294ad3c4ce1a14a184dcaf1ce2d35551242986efb7dd4cca68ea"); // no ids
 } catch(std::exception &message) {
    std::cout << message.what() << " -- OK" << std::endl;
    errors--;
 }
 try{
    c_token("$23|aaaaa$123|sbbbbbf#pass");	// bad format
 } catch(std::exception &message) {
    std::cout << message.what() << " -- OK" << std::endl;
    errors--;
 }
    if(errors == 0) {
        return false;
    } else {
        return true;
    }
}

bool test_convrt_tokenpacket() {

    std::cout << "RUNNING TEST04 CONVERTING BETWEEN TOKEN<-->PACKET" << std::endl;
    c_user A("userA"), B("userB"), C("userC"), D("userD");
    A.emit_tokens(1);

    A.send_token_bymethod(B);
    B.send_token_bymethod(C);
    C.send_token_bymethod(D);

 try {
    std::string packet = D.get_token_packet(A.get_public_key(),1);
    c_token test_tok(packet);
    std::string packet_two = test_tok.to_packet();

    if(packet == packet_two) {
        return false;
    }
    else {
        std::cout << "packet1:/n" << packet << std::endl;
        std::cout << "packet2:/n" << packet_two << std::endl;
        std::string error("error: after converting token<->packet tokens doesn't match");
        throw std::logic_error(error);
    }

 } catch(std::exception &message) {
    std::cerr << message.what() << std::endl;
    return true; // error
 }

    return false;
}

bool test_netuser() {
    std::cout << "RUNNING_NETUSER_TEST" << std::endl;
  try {
    std::string userA_name("testUser1");
    std::string userB_name("testUser2");

    c_netuser A(userA_name, 30000);
    c_netuser B(userB_name, 30001);

    A.emit_tokens(2);
    A.send_token_bynet("127.0.0.1", 30001);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    A.print_status(std::cout);
    B.print_status(std::cout);

    B.get_token_packet(A.get_public_key());	// is wallet empty?
  } catch(std::exception &ec) {
        std::cout << ec.what() << std::endl;
        return 1;
  }
    return 0;
}

bool test_rpcwallet() {
    std::cout << "RUNNING_RPCWALLET_TEST" << std::endl;
  try {
    c_user BitUser("namecoin_user");
    if (run_suite_assert (bitwallet,BitUser.check_bitwallet() == false, "wallet should be unset here!") == pfailed) return true;

    /***
     * I use my own Xcoin client for this test
     * If you want to correct use bitwallet look at your .Xcoin/Xcoin.conf for:
     * 		rpcuser
     *      rpcpassword
     *		rpcport
     *
     * Be sure you have Xcoind running before start using bitwallet
     */

    std::string rpc_usr = "nmc_testadmin";
    std::string rpc_passwd = "dontworrybehappy";
    std::string rpc_host = "127.0.0.1";
    int rpc_port = 8336;

    BitUser.set_bitwallet(rpc_usr,rpc_passwd,rpc_host,rpc_port);

    if (run_suite_assert (bitwallet,BitUser.check_bitwallet() == true, "wallet should be correctly set here!") == pfailed) return true;

    std::cout << "Your namecoin balance is: " << std::fixed << std::setprecision(8) <<
                 BitUser.get_bitwallet_balance() << std::endl;

  } catch(BitcoinException &btc_ec) {
        std::cout << btc_ec.getCode() << ": " << btc_ec.getMessage() << std::endl;
        std::cout << btc_ec.what() << std::endl;
        return 1;
  }
    return 0;
}
//////////////////////////////////////////////////////////////////////////////// WALLET_IO SUITE //////////////////////////////////////////////////////////////////////////

bool test_wallet_expected_sender() {
    std::cout << "RUNNING_WALLET_EXPECTED_SENDER_TEST" << std::endl;

  try {
    std::string userA_name("MintWallet");
    std::string userB_name("thiefUser");
    std::string userC_name("WalletUser02");
    c_user A(userA_name);
    c_user B(userB_name);
    c_user C(userC_name);
    A.emit_tokens(2);

    A.send_token_bymethod(C);

    C.save_coinwallet("./wallet.dat");
    B.load_coinwallet("./wallet.dat");

    // should detect bad expected sender
    B.send_token_bymethod(A);
    C.send_token_bymethod(A);
  } catch(std::exception &ec) {
        std::cerr << ec.what() << std::endl;
        return 1;
  }
    return 0;
}

bool test_wallet_mint_check() {
    std::cout << "RUNNING_WALLET_MINT_CHECK_TEST" << std::endl;

  try {
    std::string userA_name("MintWallet");
    std::string userB_name("thiefUser");
    c_user A(userA_name);
    c_user B(userB_name);
    A.emit_tokens(2);

    A.print_status(std::cout);

    A.save_coinwallet("./wallet.dat");
    B.load_coinwallet("./wallet.dat");

    // should detect stolen wallet database
    B.send_token_bymethod(A);
  } catch(std::exception &ec) {
        std::cerr << ec.what() << std::endl;
        return 1;
  }
    return 0;
}

int test_mint_token_expiration() {
    std::cout << "RUNNING_WALLET_MINT_CHECK_TEST" << std::endl;

  try {
    c_user A("A_user");
    A.set_new_mint("fast_tokens", A.get_public_key(), std::chrono::seconds(2));

    int token_to_emit = 5;
    A.emit_tokens(token_to_emit);
    A.print_status(std::cout);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    int expiried_tokens = A.tokens_refresh();
    A.print_status(std::cout);		// now mint and wallet should be empty
    return expiried_tokens;
    if (run_suite_assert (wallet_io, A.get_mint_last_expired_id() == token_to_emit-1 , "Last id should be different!") == pfailed) return true;
  } catch(std::exception &ec) {
        std::cerr << ec.what() << std::endl;
        return true;	// test expect return 10 (5 copy in mint + 5 in wallet)
  }
}

bool test_user_recieve_deprecated() {

    return 0;
}