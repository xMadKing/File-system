#include <iostream>
#include "fs.h"
#include <vector>
#include <math.h>


// SUPPORT FUNCTIONS
int FS::find(std::string filename){
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    for (int i = 0; i < 64; i++)
    {
        if(entries[i].file_name == filename){
            return i; 
        }
    }
    return -1; 
}

std::string get_file_name(std::string filepath){
    std::vector<std::string> directories; 
    std::string name; 
    size_t pos = 0;
    std::string token;
    std::string delimiter = "/";
    while ((pos = filepath.find(delimiter)) != std::string::npos) {
        token = filepath.substr(0, pos);
        directories.push_back(token);
        filepath.erase(0, pos + delimiter.length());
    }
    directories.push_back(filepath); 
    for(int i = 0; i < directories.size(); i++){
        name = directories[i];
    }
    return name;
}

int get_distance(std::string filepath){
    std::vector<std::string> directories; 
    std::string name; 
    size_t pos = 0;
    std::string token;
    std::string delimiter = "/";
    while ((pos = filepath.find(delimiter)) != std::string::npos) {
        token = filepath.substr(0, pos);
        directories.push_back(token);
        filepath.erase(0, pos + delimiter.length());
    }
    directories.push_back(filepath); 
    int distance = directories.size();
    return distance;
}

std::string FS::acc_to_str(int rights){
    switch (rights)
    {
    case 0x01:
        return "--x";
    case 0x02:
        return "-w-";
    case 0x03:
        return "-wx";
    case 0x04:
        return "r--";
    case 0x05:
        return "r-x";
    case 0x06:
        return "rw-";
    case 0x07:
        return "rwx";
    }
    return "---";
}


//Commands

void FS::move_to_dest(std::string dirpath)
{
    if(dirpath == "/"){
        current_dir = ROOT_BLOCK;
        return;
    }
    else if(dirpath.at(0) == '/'){
        current_dir = ROOT_BLOCK;
        dirpath.erase(0,1);
        move_to_dest(dirpath);
        return;
    }
    else {
        std::vector<std::string> directories; 
        size_t pos = 0;
        std::string token;
        std::string delimiter = "/";
        while ((pos = dirpath.find(delimiter)) != std::string::npos) {
            token = dirpath.substr(0, pos);
            directories.push_back(token);
            dirpath.erase(0, pos + delimiter.length());
        }
        directories.push_back(dirpath); 
        for(int i = 0; i < directories.size()-1; i++){
            move_to_dest(directories[i]); 
        }
        if(find(dirpath) == -1){
            return; 
        }
        dir_entry entries[64];
        disk.read(current_dir, (uint8_t*)&entries[0]);
        int dir_index; 
        for (int i = 0; i < 64; i++)
        {
            if(entries[i].file_name == dirpath){
                dir_index = i; 
            }
        }
        if(entries[dir_index].access_rights != (READ | EXECUTE) && entries[dir_index].access_rights != (WRITE | EXECUTE) &&
            entries[dir_index].access_rights != (EXECUTE) && entries[dir_index].access_rights != (READ | WRITE | EXECUTE)){
            return;
        }
        current_dir = entries[dir_index].first_blk; 
        return; 
    }
}

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
    current_dir = ROOT_BLOCK; 
}

FS::~FS()
{

}

// formats the disk, i.e., creates an empty file system
int
FS::format()
{
    uint8_t data[BLOCK_SIZE];
    dir_entry entries[64];
    for(int i = 0; i < 64; i++){
        entries[i].file_name[0] = 0;
        entries[i].access_rights = 0;
        entries[i].first_blk = 0;
        entries[i].size = 0;
        entries[i].type = 0;  
    }
    for(int i = 0; i < 56; i++){
        if(i < 2)
            entries[0].file_name[i] = '.';
        else
            entries[0].file_name[i] = 0;   
    }
    entries[0].type = TYPE_DIR; 
    entries[0].size = BLOCK_SIZE;
    entries[0].access_rights = READ + WRITE + EXECUTE;
    entries[0].first_blk = current_dir; 
    fat[FAT_BLOCK] = FAT_EOF;
    fat[ROOT_BLOCK] = FAT_EOF;
    for (int i = 0; i < BLOCK_SIZE; i++){ 
        data[i] = FAT_FREE;
    }
    for(int i = 2; i < BLOCK_SIZE/2; i++){
        fat[i] = FAT_FREE; //initializing each block with FAT_FREE, meaning a free block. Except block 0 and block 1
        disk.write(i, &data[0]);
    }
    disk.write(ROOT_BLOCK, (uint8_t*)entries);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    std::cout << "Done formatting. Disk is now ready for use." << std::endl;
    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int
FS::create(std::string filepath)
{
    int current_dir_save = current_dir;
    if(get_distance(filepath) >= 2){
        move_to_dest(filepath);
    }
    filepath = get_file_name(filepath);
        if(find(filepath) != -1){
        std::cout << "FILE ALREADY EXISTS!" << std::endl;
        return 0; 
    }
    unsigned int filesize = 0; 
    int blocks_needed = 0; 
    std::string data; 
    std::string temp;
    std::getline(std::cin, temp);
    data.append(temp);
    while(!temp.empty())
    {
        std::getline(std::cin, temp);
        data.append(temp);
    }
    data.append("\n");
    filesize = sizeof(char)*data.size(); 

    if (filesize > BLOCK_SIZE)
        blocks_needed = std::ceil(filesize/(double)BLOCK_SIZE); 

    disk.read(FAT_BLOCK, (uint8_t*)&fat[0]); 
    std::vector<int> block_vec; 
    int first_block = -1;
    if (blocks_needed > 1){
        for(int i = 0; i < blocks_needed; i++){
            for(int n = 0; n < BLOCK_SIZE/2; n++){
                if(fat[n] == FAT_FREE){
                    if(first_block == -1)
                        first_block = n; 
                    uint8_t tempdata[BLOCK_SIZE];
                    for(int x = 0; x < BLOCK_SIZE; x++){
                        try
                        {
                            tempdata[x] = data.at(x+(i*BLOCK_SIZE));
                        }
                        catch(const std::exception& e)
                        {
                            tempdata[x] = 0;
                        }
                    }
                    disk.write(n, &tempdata[0]);
                    block_vec.push_back(n);
                    fat[n] = FAT_USED; 
                    break; 
                }
            }
        }
    }
    else
    {
        uint8_t tempdata[BLOCK_SIZE]; 
        for(int n = 0; n < BLOCK_SIZE; n++){
            try
            {
                tempdata[n] = data.at(n);
            }
            catch(const std::exception& e)
            {
                tempdata[n] = 0; 
            }
        }
        for(int n = 0; n < BLOCK_SIZE/2; n++){
            if(fat[n] == FAT_FREE){
                if(first_block == -1){
                    first_block = n; 
                }
                disk.write(n, &tempdata[0]);
                fat[n] = FAT_EOF;
                break; 
            }
        }
    }
    if(blocks_needed > 1){
        for(int i = 0; i < block_vec.size()-1; i++){
            fat[block_vec[i]] = block_vec[i+1];
        }
        fat[block_vec[block_vec.size()-1]] = FAT_EOF; 
    }

    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    for(int i = 0; i < 64; i++){
        if (entries[i].file_name[0] == 0)
        {
            for(int n = 0; n < 56; n++){
                entries[i].file_name[n] = filepath[n]; 
            }
            entries[i].first_blk = first_block;
            entries[i].size = filesize;
            entries[i].type = TYPE_FILE;
            entries[i].access_rights = READ + WRITE;
            break;
        }
    }
    disk.write(current_dir, (uint8_t*)entries);
    disk.write(FAT_BLOCK, (uint8_t*)fat);//updating FAT on the disk. 
    current_dir = current_dir_save; //returning to the current dir.
    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int
FS::cat(std::string filepath)
{
    int current_dir_save = current_dir;
    if(get_distance(filepath) >= 2){
        move_to_dest(filepath);
    }
    filepath = get_file_name(filepath);
    if(find(filepath) == -1){
        std::cout << "FILE DOES NOT EXIST!" << std::endl;
        current_dir = current_dir_save;
        return 0; 
    }
    int index = find(filepath);
    dir_entry entries[64];
    std::vector<int> blocks; 
    disk.read(current_dir, (uint8_t*)&entries[0]);
    disk.read(FAT_BLOCK, (uint8_t*)&fat[0]);
    if(acc_to_str(entries[index].access_rights)[0] != 'r'){
        std::cout << "CANNOT ACCESS FILE FOR READING" << std::endl;
        current_dir = current_dir_save;
        return 0;
    }
    for(int i = 0; i < 64; i++){
        if(entries[i].file_name == filepath){
            blocks.push_back(entries[i].first_blk);
            int first_block_num = entries[i].first_blk; 
            int x = 0; 
            while(x != FAT_EOF){
                x = fat[first_block_num];
                blocks.push_back(x);
                first_block_num = x; 
            }
            blocks.pop_back(); //removing the -1 at the end. 
        }
    }
    uint8_t data[BLOCK_SIZE];
    for (int i = 0; i < blocks.size(); i++)
    {
        disk.read(blocks[i], &data[0]);
        std::cout << data << std::endl;
    }
    current_dir = current_dir_save;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int
FS::ls()
{
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    printf("NAME\t\tSIZE\t\tTYPE\t\tACCESSRIGHTS\n");
    for (int i = 0; i < 64; i++)
    {
        if (entries[i].size != 0)
        {
            if(entries[i].type == TYPE_FILE){
                std::cout << entries[i].file_name << "\t\t" << entries[i].size << "\t\t" << "file" 
                << "\t\t" << acc_to_str(entries[i].access_rights) << std::endl;
            }
            else{
                std::cout << entries[i].file_name << "\t\t" << '-' << "\t\t" << "dir" 
                << "\t\t" << acc_to_str(entries[i].access_rights) << std::endl;
            }
        }
    }
    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int
FS::cp(std::string sourcepath, std::string destpath)
{
    int current_dir_save = current_dir;
    if(get_distance(sourcepath) >= 2){
        move_to_dest(sourcepath);
    }
    sourcepath = get_file_name(sourcepath);
    if(find(sourcepath) == -1){
        std::cout << "SOURCE FILE DOES NOT EXIST!" << std::endl;
        current_dir = current_dir_save;
        return 0; 
    }
    dir_entry entries[64];
    std::vector<int> blocks; 
    disk.read(current_dir, (uint8_t*)&entries[0]);
    disk.read(FAT_BLOCK, (uint8_t*)&fat[0]);
    int source_index = find(sourcepath);
    dir_entry source_entry = entries[source_index];
    blocks.push_back(source_entry.first_blk);
    int first_block_num = source_entry.first_blk; 
    int x = 0; 
    while(x != FAT_EOF){
        x = fat[first_block_num];
        blocks.push_back(x);
        first_block_num = x; 
    }
    blocks.pop_back(); //removing the -1 at the end.
    move_to_dest(destpath);
    destpath = get_file_name(destpath);
    if(find(destpath) != -1){
        current_dir = current_dir_save;
        std::cout << "DEST FILE ALREADY EXISTS" << std::endl;
        return 0;
    }
    disk.read(current_dir, (uint8_t*)&entries[0]);
    int first_blk = -1; 
    uint8_t data[BLOCK_SIZE];
    if(blocks.size() == 1){
        disk.read(blocks[0], &data[0]);
        for(int i = 0; i < BLOCK_SIZE/2; i++){
            if(fat[i] == FAT_FREE){
                disk.write(i, &data[0]);
                first_blk = i; 
                fat[i] = -1; 
                break; 
            }
        }
        for(int i = 0; i < 64; i++){
            if(entries[i].size == 0){
                for(int n = 0; n < 56; n++){
                    entries[i].file_name[n] = destpath[n];
                }
                entries[i].access_rights = source_entry.access_rights;
                entries[i].size = source_entry.size;
                entries[i].type = source_entry.type; 
                entries[i].first_blk = first_blk; 
                break; 
            }
        }
        disk.write(current_dir, (uint8_t*)&entries);
        disk.write(FAT_BLOCK, (uint8_t*)&fat);
    }
    else
    {
        std::vector<int> temp_vec; 
        for (int i = 0; i < blocks.size(); i++)
        {
            disk.read(blocks.at(i), &data[0]); 
            for(int n = 0; n < BLOCK_SIZE/2; n++){
                if(fat[n] == FAT_FREE){
                    temp_vec.push_back(n);
                    disk.write(n, &data[0]);
                    if (first_blk == -1)
                    {
                        first_blk = n; 
                    }
                    fat[n] = FAT_USED; 
                    break;
                }
            }
        }
        /*UPDATING FAT*/
        for(int i = 0; i < temp_vec.size()-1; i++){
            fat[temp_vec[i]] = temp_vec[i+1];
        }
        fat[temp_vec[temp_vec.size()-1]] = FAT_EOF; 
        /*UPDATING ROOTDIR*/
        for(int i = 0; i < 64; i++){
            if(entries[i].size == 0){
                for(int n = 0; n < 56; n++){
                    entries[i].file_name[n] = destpath[n];
                }
                entries[i].access_rights = source_entry.access_rights;
                entries[i].size = source_entry.size;
                entries[i].type = source_entry.type; 
                entries[i].first_blk = first_blk; 
                break; 
            }
        }
        disk.write(current_dir, (uint8_t*)entries);
        disk.write(FAT_BLOCK, (uint8_t*)fat);
    }
    current_dir = current_dir_save;
    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int
FS::mv(std::string sourcepath, std::string destpath)
{
    int current_dir_save = current_dir;
    if(get_distance(sourcepath) >= 2){
        move_to_dest(sourcepath);
    }
    sourcepath = get_file_name(sourcepath);
    dir_entry source_entry;
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    for (int i = 0; i < 64; i++)
    {
        if(entries[i].file_name == sourcepath){
            source_entry.first_blk = entries[i].first_blk;
            entries[i].first_blk = 0;
            source_entry.size = entries[i].size;
            entries[i].size = 0;
            source_entry.type = entries[i].type;
            entries[i].type = 0;
            for(int n = 0; n < 56; n++){
                source_entry.file_name[n] = entries[i].file_name[n];
            }
            entries[i].file_name[0] = 0;
            source_entry.access_rights = entries[i].access_rights;
            entries[i].access_rights = 0;
            break;
        }
    }
    disk.write(current_dir, (uint8_t*)&entries[0]);
    move_to_dest(destpath);
    destpath = get_file_name(destpath);
    if (find(destpath) != -1){
        std::cout << "FILE NAME ALREADY EXISTS! RENAMED FILE TO " << destpath << "_1" << std::endl;
        destpath = destpath + "_1"; 
    }
    disk.read(current_dir, (uint8_t*)&entries[0]);
    for(int i = 0; i < 64; i++){
        if (entries[i].file_name[0] == 0)
        {
            for(int n = 0; n < 56; n++){
                entries[i].file_name[n] = destpath[n]; 
            }
            entries[i].first_blk = source_entry.first_blk;
            entries[i].size = source_entry.size;
            entries[i].type = source_entry.type;
            entries[i].access_rights = source_entry.access_rights;
            break;
        }
    }
    disk.write(current_dir, (uint8_t*)&entries[0]);
    current_dir = current_dir_save;
    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int
FS::rm(std::string filepath)
{
    int current_dir_save = current_dir;
    if(get_distance(filepath) >= 2){
        move_to_dest(filepath);
    }
    filepath = get_file_name(filepath);
    if(find(filepath) == -1){
        std::cout << "FILE DOES NOT EXIST!" << std::endl;
        return 0; 
    }
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    disk.read(FAT_BLOCK, (uint8_t*)&fat[0]);
    int rm_entry_index = find(filepath);
    if(entries[rm_entry_index].type == TYPE_DIR){
        dir_entry rm_dir[64];
        disk.read(entries[rm_entry_index].first_blk, (uint8_t*)rm_dir);
        int x = 0;
        for(int i = 1; i < 64; i++) // first entry in a sub directory is always taken by a reference to the parent directory
        {
            if(rm_dir[i].file_name[0] == 0){
                x++; 
            }
        }
        if(x < 63)
        {
            std::cout << "ERROR: DIRECTORY NOT EMPTY!" << std::endl;
            return 0; 
        }
    }
    int x = FAT_USED; 
    while(x != -1){
        x = fat[entries[rm_entry_index].first_blk];
        fat[entries[rm_entry_index].first_blk] = FAT_FREE;
        entries[rm_entry_index].first_blk = x; 
    }
    entries[rm_entry_index].first_blk = FAT_FREE;
    for (int i = 0; i < 56; i++)
    {
        entries[rm_entry_index].file_name[i] = 0; 
    }
    entries[rm_entry_index].access_rights = 0; 
    entries[rm_entry_index].size = 0;
    entries[rm_entry_index].type = 0;
    disk.write(current_dir, (uint8_t*)entries);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    current_dir = current_dir_save; 
    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int
FS::append(std::string filepath1, std::string filepath2)
{
    int current_dir_save = current_dir; 
    move_to_dest(filepath1);
    filepath1 = get_file_name(filepath1);
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries);
    if(find(filepath1) == -1){
        std::cout << "FILE " <<  filepath1 << " DOES NOT EXIST!" << std::endl;
        current_dir = current_dir_save;
        return 0; 
    }
    int index1 = find(filepath1);
    dir_entry file1 = entries[index1];
    if(acc_to_str(file1.access_rights)[0] != 'r'){
        std::cout << "NO PERMISSON! TO READ FILE1." << std::endl;
        current_dir = current_dir_save;
        return 0; 
    }
    std::vector<int> blocks_1; 
    int first_block_num = file1.first_blk; 
    int x = 0; 
    while(x != FAT_EOF){
        x = fat[first_block_num];
        blocks_1.push_back(first_block_num);
        first_block_num = x; 
    }
    uint8_t file1_data[blocks_1.size()*BLOCK_SIZE];
    for(int i = 0; i < blocks_1.size(); i++){
        uint8_t tempdata[BLOCK_SIZE];
        disk.read(blocks_1[i], &tempdata[0]);
        for(int n = 0+(i*BLOCK_SIZE); n < BLOCK_SIZE*blocks_1.size(); n++){
            file1_data[n] = tempdata[n%BLOCK_SIZE];
        }
    }
    move_to_dest(filepath2);
    filepath2 = get_file_name(filepath2);
    entries[64];
    disk.read(current_dir, (uint8_t*)&entries);
    if(find(filepath2) == -1){
        std::cout << "FILE " <<  filepath2 << " DOES NOT EXIST!" << std::endl;
        return 0; 
    }
    int index2 = find(filepath2);
    dir_entry file2 = entries[index2];
    if( acc_to_str(file2.access_rights)[0] != 'r' 
    && acc_to_str(file2.access_rights)[1] != 'w'){
        current_dir = current_dir_save;
        std::cout << "NO PERMISSION TO READ/WRITE TO FILE2" << std::endl;
        return 0; 
    }
    std::vector<int> blocks_2; 
    first_block_num = file2.first_blk; 
    x = 0; 
    while(x != FAT_EOF){
        x = fat[first_block_num];
        blocks_2.push_back(first_block_num);
        first_block_num = x; 
    }
    uint8_t file2_last_block[BLOCK_SIZE+(BLOCK_SIZE*blocks_1.size())];
    disk.read(blocks_2[blocks_2.size()-1], file2_last_block);
    x = 0; 
    for(int i = 0; i < BLOCK_SIZE+(BLOCK_SIZE*blocks_1.size()); i++){
        if (file2_last_block[i] != 0) {
            x++;
        }
        else {
            file2_last_block[i] = file1_data[i-x]; 
        }
    }
    int filesize = file2.size + file1.size;
    int blocks_to_update; 
    if(filesize < BLOCK_SIZE){
        blocks_to_update = blocks_1.size();
    }
    else{
        blocks_to_update  = blocks_1.size()+1; 
    }
    for(int i = 0; i < blocks_to_update; i++){
        if(i == 0){
            uint8_t tempdata[BLOCK_SIZE];
            for (int n = 0; n < BLOCK_SIZE; n++)
            {
                tempdata[n] = file2_last_block[n];
            }
            disk.write(blocks_2[blocks_2.size()-1], &tempdata[0]);
        }
        else
        {
            uint8_t tempdata[BLOCK_SIZE];
            for (int n = 0+(i*BLOCK_SIZE); n < BLOCK_SIZE*(blocks_1.size()+1); n++)
            {
               tempdata[n%BLOCK_SIZE] = file2_last_block[n];
            }
            for (int n = 0; n < BLOCK_SIZE/2; n++)
            {
                if (fat[n] == FAT_FREE)
                {
                    disk.write(n, &tempdata[0]);
                    blocks_2.push_back(n);
                    fat[n] = FAT_USED; 
                    break; 
                }
            }
        }
    }
    if(blocks_2.size() > 1){
        for(int i = 0; i < blocks_2.size()-1; i++){
            fat[blocks_2[i]] = blocks_2[i+1];
        }
        fat[blocks_2[blocks_2.size()-1]] = FAT_EOF; 
    }
    file2.size = filesize;
    entries[index2] = file2;
    disk.write(current_dir, (uint8_t*)entries);
    current_dir = current_dir_save;
    disk.write(FAT_BLOCK, (uint8_t*)fat); 
    return 0;
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int
FS::mkdir(std::string dirpath)
{
    int current_dir_save = current_dir;
    if(get_distance(dirpath) >= 2){
        move_to_dest(dirpath);
    }
    dirpath = get_file_name(dirpath);
    if(find(dirpath) != -1){
        std::cout << "NAME IS ALREADY IN USE!" << std::endl; 
        return 0; 
    }
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    disk.read(FAT_BLOCK, (uint8_t*)&fat[0]);
    int write_index; 
    for (int i = 0; i < 64; i++)
    {
        if(entries[i].file_name[0] == 0){
            write_index = i; 
            break; 
        }
    }
    dir_entry new_dir[64];
    for(int i = 0; i < 64; i++){
        new_dir[i].file_name[0] = 0; 
        new_dir[i].type = 0;
        new_dir[i].size = 0; 
    }
    for(int i = 0; i < 56; i++){
        if(i < 2)
            new_dir[0].file_name[i] = '.';
        else
            new_dir[0].file_name[i] = 0;  
        entries[write_index].file_name[i] = dirpath[i]; 
    }
    new_dir[0].type = TYPE_DIR; 
    new_dir[0].size = BLOCK_SIZE;
    new_dir[0].access_rights = READ + WRITE + EXECUTE;
    new_dir[0].first_blk = current_dir; 
    entries[write_index].size = BLOCK_SIZE;
    entries[write_index].access_rights = READ + WRITE + EXECUTE;
    entries[write_index].type = TYPE_DIR;
    for (int i = 0; i < BLOCK_SIZE/2; i++)
    {
        if(fat[i] == FAT_FREE){
            entries[write_index].first_blk = i; 
            disk.write(i, (uint8_t*)new_dir);
            fat[i] = FAT_EOF;
            break; 
        }
    }
    disk.write(current_dir, (uint8_t*)entries);
    disk.write(FAT_BLOCK, (uint8_t*)fat);
    disk.write(entries[write_index].first_blk, (uint8_t*)new_dir); 
    current_dir = current_dir_save;
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int
FS::cd(std::string dirpath)
{
    if(dirpath == "/"){
        current_dir = ROOT_BLOCK;
        return 0;
    }
    else if(dirpath.at(0) == '/'){
        current_dir = ROOT_BLOCK;
        dirpath.erase(0,1);
        cd(dirpath);
        return 0;
    }
    else {
        std::vector<std::string> directories; 
        size_t pos = 0;
        std::string token;
        std::string delimiter = "/";
        while ((pos = dirpath.find(delimiter)) != std::string::npos) {
            token = dirpath.substr(0, pos);
            directories.push_back(token);
            dirpath.erase(0, pos + delimiter.length());
        }
        directories.push_back(dirpath); 
        for(int i = 0; i < directories.size()-1; i++){
            cd(directories[i]); 
        }
        if(find(dirpath) == -1){
            std::cout << "DIR NOT FOUND!" << std::endl;
            return 0; 
        }
        dir_entry entries[64];
        disk.read(current_dir, (uint8_t*)&entries[0]);
        int dir_index; 
        for (int i = 0; i < 64; i++)
        {
            if(entries[i].file_name == dirpath){
                dir_index = i; 
            }
        }
        if(entries[dir_index].access_rights != (READ | EXECUTE) && entries[dir_index].access_rights != (WRITE | EXECUTE) &&
            entries[dir_index].access_rights != (EXECUTE) && entries[dir_index].access_rights != (READ | WRITE | EXECUTE)){
            std::cout << "NO PERMISSION TO ENTER FILE, PLACED IN CLOSEST DIRECTORY" << std::endl;
            return 0;
        }
        current_dir = entries[dir_index].first_blk; 
        return 0; 
    }
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int
FS::pwd()
{
    if (current_dir == ROOT_BLOCK)
    {
        std::cout << '/' << std::endl; 
    }
    else
    {
        std::vector<std::string> path;
        path.push_back("/");
        int current_dir_cpy = current_dir;
        while(current_dir_cpy != ROOT_BLOCK){
            dir_entry entries[64];
            disk.read(current_dir_cpy, (uint8_t*)entries);
            dir_entry p_entries[64];
            disk.read(entries[0].first_blk, (uint8_t*)p_entries);
            for (int i = 0; i < 64; i++)
            {
                if(p_entries[i].first_blk == current_dir_cpy){
                    path.push_back(p_entries[i].file_name);
                    path.push_back("/");
                    current_dir_cpy = entries[0].first_blk; 
                    break; 
                }
            }
        }
        for(int i = path.size(); i > 0; i--){
            std::cout << path[i];
        }
        std::cout << "\n"; 
    }
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int
FS::chmod(std::string accessrights, std::string filepath)
{
    int current_dir_save = current_dir;
    if(get_distance(filepath) >= 2){
        move_to_dest(filepath);
    }
    filepath = get_file_name(filepath);
    dir_entry entries[64];
    disk.read(current_dir, (uint8_t*)&entries[0]);
    for(int i = 0; i < 64; i++){
        if(entries[i].file_name == filepath){
            entries[i].access_rights = std::stoi(accessrights); 
        }
    }
    disk.write(current_dir, (uint8_t*)entries);
    current_dir = current_dir_save;
    return 0;
}
