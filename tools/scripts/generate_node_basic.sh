#/bin/bash
set -e

LOG_ERROR()
{
    local content=${1}
    echo -e "\033[31m"${content}"\033[0m"
}

LOG_INFO()
{
    local content=${1}
    echo -e "\033[34m"${content}"\033[0m"
}

yes_go_other_exit()
{
    read -r -p "[Y/n]: " input
    case $input in
        [yY][eE][sS]|[yY])
            ;;

        [nN][oO]|[nN])
            exit 1
                ;;

        *)
        exit 1
        ;;
    esac    
}

output_dir=
name=
listenip=127.0.0.1
rpcport=
p2pport=
channelPort=
peers=
systemproxyaddress=0x0
enable_guomi=0

this_script=$0
help() {
    LOG_ERROR "${1}"
    echos "$this_script: Generate node basic files except for cert files"
    echos "Usage:"
    echos "    -o  <output dir>          Where node files generate "
    echos "    -n  <node name>           Name of node"
    echos "    -l  <listen ip>           Node's listen IP"
    echos "    -r  <RPC port>            Node's RPC port"
    echos "    -p  <P2P port>            Node's P2P port"
    echos "    -c  <channel port>        Node's channel port"
    echos "    -e  <bootstrapnodes>      Node's bootstrap nodes"
    echos "Optional:"
    echos "    -x  <systemproxyaddress>  System Proxy Contract address"
    echos "     -g                       Create guomi node"
    echos "    -h                        This help"
    echos "Example:"
    echos "    bash $this_script -o /mydata -n node0 -l 127.0.0.1 -r 8545 -p 30303 -c 8891 -e 127.0.0.1:30303,127.0.0.1:30304"
    echos "    bash $this_script -o /mydata -n node0 -l 127.0.0.1 -r 8545 -p 30303 -c 8891 -e 127.0.0.1:30303,127.0.0.1:30304 -s 0x210a7d467c3c43307f11eda35f387be456334fed"
    echos "Guomi Example:"
    echos "    bash $this_script -o ~/mydata -n node0 -l 127.0.0.1 -r 8545 -p 30303 -c 8891 -e 127.0.0.1:30303,127.0.0.1:30304 -g"
    echos "    bash $this_script -o ~/mydata -n node0 -l 127.0.0.1 -r 8545 -p 30303 -c 8891 -e 127.0.0.1:30303,127.0.0.1:30304 -s 0x210a7d467c3c43307f11eda35f387be456334fed -g"

exit -1
}

while getopts "o:n:l:r:p:c:e:x:gh" option;do
	case $option in
	o) output_dir=$OPTARG;;
    n) name=$OPTARG;;
    l) listenip=$OPTARG;;
    r) rpcport=$OPTARG;;
    p) p2pport=$OPTARG;;
    c) channelPort=$OPTARG;;
    e) peers=$OPTARG;;
    x) systemproxyaddress=$OPTARG;;
    g) enable_guomi=1;;
	h) help;;
	esac
done

[ -z $output_dir ] && help 'Error! Please specify <output dir> using -o'
[ -z $name ] && help 'Error! Please specify <node name> using -z'
[ -z $listenip ] && help 'Error! Please specify <listen ip> using -l'
[ -z $rpcport ] && help 'Error! Please specify <RPC port> using -r'
[ -z $p2pport ] && help 'Error! Please specify <P2P port> using -p'
[ -z $channelPort ] && help 'Error! Please specify <channel port> using -c'
[ -z $peers ] && help 'Error! Please specify <bootstrapnodes> using -e'

node_dir=$output_dir/$name/

check_node_running() {
    node_dir=$1
    pre_dir=`pwd`
    cd $node_dir

    #Is the node running?
    name=`pwd`/config.json
    agent_pid=`ps aux|grep "$name"|grep -v grep|awk '{print $2}'`
    if [ $agent_pid ]; then
        echo "Attention! Node is running. Node will be destroied. Continue?"
        yes_go_other_exit
        set +e && bash stop.sh && set -e
    fi
    cd $pre_dir
}

generate_nodedir() {
    out=$1
    mkdir -p $out
    mkdir -p $out/data/
    mkdir -p $out/log/
    mkdir -p $out/keystore/

    LOG_INFO "`readlink -f $out/data ` is generated"
    LOG_INFO "`readlink -f $out/log` is generated"
    LOG_INFO "`readlink -f $out/keystore` is generated"
}

generate_node_script() {
    out=$1
    cp start.sh stop.sh $out
    chmod +x $out/start.sh
    chmod +x $out/stop.sh

    LOG_INFO "`readlink -f $out/start.sh` is generated"
    LOG_INFO "`readlink -f $out/stop.sh` is generated"
}

generate_config() {
    out=$1
    mkdir -p $out
    out_file=$out/config.json
    echo '{
        "sealEngine": "PBFT",
        "systemproxyaddress":"'$systemproxyaddress'",
        "listenip":"'$listenip'",
        "cryptomod":"0",
        "rpcport": "'$rpcport'",
        "p2pport": "'$p2pport'",
        "channelPort": "'$channelPort'",
        "wallet":"./data/keys.info",
        "keystoredir":"./data/keystore/",
        "datadir":"./data/",
        "vm":"interpreter",
        "networkid":"12345",
        "logverbosity":"4",
        "coverlog":"OFF",
        "eventlog":"OFF",
        "statlog":"ON",
        "logconf":"./log.conf"
}' > $out_file
    LOG_INFO "`readlink -f $out_file` is generated"
}

generate_logconf() {
    out=$1
    mkdir -p $out
    out_file=$out/log.conf    
    echo '* GLOBAL:  
    ENABLED                 =   true  
    TO_FILE                 =   true  
    TO_STANDARD_OUTPUT      =   false  
    FORMAT                  =   "%level|%datetime{%Y-%M-%d %H:%m:%s:%g}|%msg"   
    FILENAME                =   "./log/log_%datetime{%Y%M%d%H}.log"  
    MILLISECONDS_WIDTH      =   3  
    PERFORMANCE_TRACKING    =   false  
    MAX_LOG_FILE_SIZE       =   209715200 ## 200MB - Comment starts with two hashes (##)
    LOG_FLUSH_THRESHOLD     =   100  ## Flush after every 100 logs

* FATAL:  
    ENABLED                 =   false
    TO_FILE                 =   false
    
* ERROR:  
    ENABLED                 =   true
    TO_FILE                 =   false
      
* WARNING: 
     ENABLED                 =   true
     TO_FILE                 =   false
 
* INFO: 
    ENABLED                 =   true
    TO_FILE                 =   false 
      
* DEBUG:  
    ENABLED                 =   false
    TO_FILE                 =   false
      
* TRACE:  
    ENABLED                 =   false
    TO_FILE                 =   false

* VERBOSE:  
    ENABLED                 =   false
    TO_FILE                 =   false
' > $out_file
    LOG_INFO "`readlink -f $out_file` is generated"
}

generate_bootstrapnodes() {
    out=$1/data
    mkdir -p $out 
    out_file=$out/bootstrapnodes.json
    rm -f $out_file 

    not_first=
    bootstrapnodes={\"nodes\":[
    #foreach peers
    IFS=',' read -ra PEERS <<< "$peers"
    for PEER in "${PEERS[@]}"; do
        IFS=':' read -ra URL <<< "$PEER"
        ip=${URL[0]}
        port=${URL[1]}
        if [ -z $not_first ]; then
            bootstrapnodes=$bootstrapnodes\{\"host\":\"$ip\",\"p2pport\":\"$port\"\}
            not_first="yes"
        else
            bootstrapnodes=$bootstrapnodes\,\{\"host\":\"$ip\",\"p2pport\":\"$port\"\}
        fi
    done
    bootstrapnodes=$bootstrapnodes\]\}
    echo $bootstrapnodes > $out_file
    LOG_INFO "`readlink -f $out_file` is generated"
}

if [ -d "$node_dir" ] && [ ${enable_guomi} -eq 0 ]; then
    echo "Attention! Duplicate generation of \"$node_dir\". Re-generate?(This will remove all old data of the node)"
    yes_go_other_exit

    check_node_running $node_dir

    rm -rf $node_dir
fi

generate_nodedir $node_dir
generate_node_script $node_dir
generate_config $node_dir
generate_logconf $node_dir
generate_bootstrapnodes $node_dir

LOG_INFO "Generate success!"
