#include"comptool.h"

int main(int argc, char** argv){
    CompTool comp;
    comp.run_command(argc, argv);
    return 0;
}


string basename(const string& path){
    return path.substr(path.find_last_of('/') + 1);
}


void CompTool::run_command(int argc, char** argv){
    argc--; argv++;
    const string command = argv[0];
    if(command == "align") {
        int* SA = create_SA(argv[1]);
        search_alignment(argc, argv, SA);
        delete SA;
    }
    else if(command == "chain") {
        chain_alignment(argc, argv);
    }
    else if(command == "all") {
        int* SA = create_SA(argv[1]);
        search_alignment(argc, argv, SA);
        delete SA;
        chain_alignment(argc, argv);
    }
    else {
        cout << " Invalid command > \"" << command << "\"" << endl;
    }
}


int* CompTool::create_SA(const char* file){
    // Perform induced sorting with int array
    int* seq1_int = read_fasta_and_create_int_array(file, seq1_size_);
    int* SA = new int[seq1_size_];
    IS is(seq1_int, SA, seq1_size_, num_char_);
    is.run();

    seq1_ = new int8_t[seq1_size_];
    copy_int_array_to_int8_t_array(seq1_int, seq1_, seq1_size_);
    delete seq1_int;
    return SA;
}


// argv[1]: seq1, argv[2]: seq2
// (OPTION)
// [-k k-mer] : k-mer
// [-l slide_letters] : k-mer の query を取るとき何文字ずつずらすか
// [-m max_num_matches] : query に対するマッチの数が何個のものまで出力するか
// [-i] : bwt_interval
// [-s] : outputs start position
// [-f] : searches forward only
// [-b] : searches backward only
void CompTool::search_alignment(int argc, char** argv, int* SA){
    if(argc < 3) {cout << "File not enough" << endl; exit(1);}

    const string seq1_file = argv[1];
    const string seq2_file = argv[2];

    // Set default values for options
    int k = 15;
    int bwt_interval = 1;
    int slide_letters = 1;
    int max_num_matches = 1000000000;
    bool search_forward = true;
    bool search_backward = true;
    bool outputs_start_pos = false;

    if(argc > 3){
        for(int i = 3; i < argc; i++){
            if(argv[i][1] == 'k')       k                   = atoi(argv[++i]);
            else if(argv[i][1] == 'l')  slide_letters       = atoi(argv[++i]);
            else if(argv[i][1] == 'm')  max_num_matches     = atoi(argv[++i]);
            else if(argv[i][1] == 'i')  bwt_interval        = atoi(argv[++i]);
            else if(argv[i][1] == 'f')  search_backward     = false;
            else if(argv[i][1] == 'b')  search_forward      = false;
            else if(argv[i][1] == 's')  outputs_start_pos   = true;
        }
    }
    // Create BWT from seq1
    BWT bwt(seq1_, SA, seq1_size_, num_char_, bwt_interval);

    seq2_ = read_fasta_and_create_int8_t_array(seq2_file, seq2_size_);

    // Search forward matches
    if(search_forward){
        const string seq1_name = basename(seq1_file);
        const string seq2_name = basename(seq2_file);
        stringstream out_file;
        if(outputs_start_pos)
            out_file << "alignments-forward-startpos_" << seq1_name << "_" << seq2_name << ".tsv";
        else
            out_file << "alignments-forward-for-chaining_" << seq1_name << "_" << seq2_name << ".tsv";
        ofstream ofs(out_file.str().c_str());

        ofs << "#" << seq2_file << "\t" << seq1_file << endl;  // header
        for(int i = 0; i < seq2_size_ - k; i += slide_letters){
            int8_t* query = &seq2_[i];
            int lb, ub;  // lower-bound and upper-bound of matches in suffix array
            bwt.search(query, k, lb, ub);
            if(lb <= ub){
                for(int j = lb; j <= ub; j++){
                    if(j == lb + max_num_matches) break;
                    if(outputs_start_pos)
                        output_startpos(ofs, i, SA[j]);
                    else
                        output_alignment_forward(ofs, i, SA[j], k);
                }
            }
        }
    }

    // Search reverse complement matches
    if(search_backward){
        const string seq1_name = basename(seq1_file);
        const string seq2_name = basename(seq2_file);
        stringstream out_file;
        if(outputs_start_pos)
            out_file << "alignments-backward-startpos_" << seq1_name << "_" << seq2_name << ".tsv";
        else
            out_file << "alignments-backward-for-chaining_" << seq1_name << "_" << seq2_name << ".tsv";
        ofstream ofs(out_file.str().c_str());

        ofs << "#" << seq2_file << "\t" << seq1_file << endl;  // header
        int8_t* query = new int8_t[k];
        for(int i = k-1; i < seq2_size_ - 1; i += slide_letters){
            // convert k-mers to reverse complements
            for(int j = 0; j < k; j++)
                query[j] = num_char_ - seq2_[i-j];

            int lb, ub;  // lower-bound and upper-bound of matches in suffix array
            bwt.search(query, k, lb, ub);
            if(lb <= ub){
                for(int j = lb; j <= ub; j++){
                    if(j == lb + max_num_matches) break;
                    if(outputs_start_pos)
                        output_startpos(ofs, i, SA[j]);
                    else
                        output_alignment_backward(ofs, i, SA[j], k);
                }
            }
        }
        delete[] query;
    }
}


// argv[1]: seq1, argv[2]: seq2
// (OPTION) [-n]: near_dist
// [-f]: runs forward only, [-b]: runs backward only
void CompTool::chain_alignment(int argc, char** argv){
    if(argc < 3) {cout << "Files not enough" << endl; exit(1);}
    // OPTION
    int near_dist = 50;
    bool runs_forward = true;
    bool runs_backward = true;
    if(argc > 3){
        for(int i = 3; i < argc; i++)
            if(argv[i][1] == 'n')  near_dist = atoi(argv[++i]);
            else if(argv[i][1] == 'f')  runs_backward = false;
            else if(argv[i][1] == 'b')  runs_forward = false;
    }

    const string seq1_file = argv[1];
    const string seq2_file = argv[2];

    const string seq1_name = basename(seq1_file);
    const string seq2_name = basename(seq2_file);

    stringstream out_file;
    out_file << "chains_" << seq1_name << "_" << seq2_name << ".tsv";
    ofstream ofs(out_file.str().c_str());
    ofs << "#" << seq2_name << "\t" << seq1_name << endl;

    if(runs_forward){
        stringstream ifs1_name;
        ifs1_name << "alignments-forward-for-chaining_" << seq1_name << "_" << seq2_name
                  << ".tsv";
        run_chaining(ifs1_name.str(), ofs, near_dist);
    }

    if(runs_backward){
        stringstream ifs2_name;
        ifs2_name << "alignments-backward-for-chaining_" << seq1_name << "_" << seq2_name
                  << ".tsv";
        run_chaining(ifs2_name.str(), ofs, near_dist);
    }
}


// Receive an alignment file, run chaining and output a chain file
void CompTool::run_chaining(string file, ofstream& ofs, const int near_dist){
    // Count alignments
    ifstream ifs;
    ifs.open(file.c_str());
    if(!ifs.is_open()) {cout << "File not found\n> " << file << endl; exit(1);}
    string line;
    int num_alignment = 0;
    getline(ifs, line);  // Skip header
    while(getline(ifs, line)) num_alignment++;
    ifs.close();

    // Load alignments
    ifs.open(file.c_str());
    getline(ifs, line);  // Skip header
    Alignment* alignments = new Alignment[num_alignment];
    int* buf = new int[5];
    for(int i = 0; i < num_alignment; i++){
        for(int j = 0; j < 5; j++){
            ifs >> buf[j];
        }
        alignments[i].set(buf);
    }
    delete[] buf;

    // Run chaining
    Chaining chaining(alignments, num_alignment, near_dist);
    chaining.run();
    chaining.output_major_chains(ofs);
}


// fastaファイルを読み込んで必要なメモリを確保し、'ACGTN'を'12340'に変換して格納する (int版)
int* CompTool::read_fasta_and_create_int_array(const char* file, int& size){
    ifstream ifs;
    ifs.open(file);
    if(!ifs.is_open()) {cout << "File not found\n>" << file << endl; exit(1);}
    string header;
    getline(ifs, header);   // ヘッダーを読み込む
    size = 0;
    char buf;
    while(ifs >> buf) size++; // 文字数をカウント
    ifs.close();

    // 再びファイルを開いて1文字ずつ読み込み、数字にエンコードして配列に格納
    ifs.open(file);
    getline(ifs, header);
    int* s = new int[++size];
    int* p = s;
    while(ifs >> buf) *p++ = encode_char(buf);
    *p++ = 0;   // 末尾に'0'を入れる
    return s;
}


// fastaファイルを読み込んで必要なメモリを確保し、'ACGTN'を'12340'に変換して格納する (int8_t版)
int8_t* CompTool::read_fasta_and_create_int8_t_array(const string& file, int& size){
    ifstream ifs;
    ifs.open(file);
    if(!ifs.is_open()) {cout << "File not found\n>" << file << endl; exit(1);}
    string header;
    getline(ifs, header);   // ヘッダーを読み込む
    size = 0;
    char buf;
    while(ifs >> buf) size++; // 文字数をカウント
    ifs.close();

    // 再びファイルを開いて1文字ずつ読み込み、数字にエンコードして配列に格納
    ifs.open(file);
    getline(ifs, header);
    int8_t* s = new int8_t[++size];
    int8_t* p = s;
    while(ifs >> buf) *p++ = (int8_t)encode_char(buf);
    *p++ = 0;   // 末尾に'0'を入れる
    return s;
}


// intの配列の中身をint8_tの配列にコピーする
void CompTool::copy_int_array_to_int8_t_array(int* int_array, int8_t* int8_array, const int size){
    for(int i = 0 ; i < size; i++)
        int8_array[i] = int_array[i];
}
