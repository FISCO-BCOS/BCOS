#!/bin/bash
set -e

function LOG_ERROR()
{
    local content=${1}
    echo -e "\033[31m"${content}"\033[0m"
}

function LOG_INFO()
{
    local content=${1}
    echo -e "\033[34m"${content}"\033[0m"
}

function execute_cmd()
{
    local command="${1}"
    eval ${command}
    local ret=$?
    if [ $ret -ne 0 ];then
        LOG_ERROR "execute command ${command} FAILED"
        exit 1
    else
        LOG_INFO "execute command ${command} SUCCESS"
    fi
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

if [ "" = "`openssl ecparam -list_curves | grep secp256k1`" ];
then
    LOG_ERROR "Current Openssl Don't Support secp256k1 ! Please Upgrade Openssl To  OpenSSL 1.0.2k-fips"
    exit;
fi

agency_name=
agency_dir=
node_name=
sdk_name=
output_dir=
manual_input=
gen_guomi_cert=0

this_script=$0
help() {
    LOG_ERROR "${1}"
    LOG_INFO "Usage:"
    LOG_INFO "    -a  <agency name>   The agency name that the node belongs to"
    LOG_INFO "    -d  <agency dir>    The agency cert dir that the node belongs to"
    LOG_INFO "    -n  <node name>     Node name"
    LOG_INFO "    -o  <output dir>    Data dir of the node"
    LOG_INFO "    -r  <GM shell path> The path of GM shell scripts directory"
    LOG_INFO "    -s  <sdk name>      The sdk name to connect with the node "
    LOG_INFO "    -g 			      Generate guomi cert"
    LOG_INFO "    -m                  Input agency information manually"
    LOG_INFO "    -h                  This help"
    LOG_INFO "Example:"
    LOG_INFO "    bash $this_script -a test_agency -d /mydata/test_agency -n node0 -o /mydata/node0/data"
    LOG_INFO "    bash $this_script -a test_agency -d /mydata/test_agency -n node0 -o /mydata/node0/data -m"
    LOG_INFO "guomi Example:"
    LOG_INFO "    bash $this_script -a test_agency -d /mydata/test_agency -n node0 -o /mydata/node0/data -s sdk1 -g"
    LOG_INFO "    bash $this_script -a test_agency -d /mydata/test_agency -n node0 -o /mydata/node0/data -m -s sdk1 -g"
exit -1
}

guomi_dir=../cert/GM
while getopts "a:d:n:o:s:r:gmh" option;do
	case $option in
    a) agency_name=$OPTARG;;
    d) agency_dir=$OPTARG;;
    n) node_name=$OPTARG;;
	o) output_dir=$OPTARG;;
    s) sdk_name=$OPTARG;;
    m) manual_input="yes";; 
    r) guomi_dir=${OPTARG};;
	h) help;;
    g) gen_guomi_cert=1;;
	esac
done

[ -z $agency_name ] && help 'Error: Please specify <agency dir> using -a'
[ -z $agency_dir ] && LOG_INFO 'Error: Please specify <agency dir> using -d, using ${agency_name} by default' && agency_dir="${agency_name}"
[ -z $node_name ] && help 'Error: Please specify <agency dir> using -d'
[ -z $output_dir ] && LOG_INFO 'Error: Please specify <output dir> using -o, using ${node_name} by deafult' && node_dir="${node_name}"
if [ ${gen_guomi_cert} -eq 1 ] && [ "${sdk_name}" == "" ]; then
    help "Please Specify sdk_name when generating guomi certificates"
fi
generate_cert_config() {
    out=$1
    echo '
[ca]
default_ca=default_ca
[default_ca]
default_days = 365
default_md = sha256
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
[req_distinguished_name]
countryName = CN
countryName_default = CN
stateOrProvinceName = State or Province Name (full name)
stateOrProvinceName_default =GuangDong
localityName = Locality Name (eg, city)
localityName_default = ShenZhen
organizationalUnitName = Organizational Unit Name (eg, section)
organizationalUnitName_default = test_org
commonName =  Organizational  commonName (eg, test_org)
commonName_default = test_org
commonName_max = 64
[ v3_req ]
# Extensions to add to a certificate request
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
[ v4_req ]
basicConstraints = CA:TRUE
' > $out/cert.cnf
}

current_dir=`pwd`
function gen_guomi_cert_func(){
    local agency_name=${1}
    local agency_dir=${2}
    local node_name=${3}
    local node_dir=${4}
    local manual="no"
    if [ "${manual_input}" == "yes" ];then
        manual="yes"
    fi
    execute_cmd "cd ${guomi_dir}"
    execute_cmd "./gmnode.sh ${agency_name} ${agency_dir} ${node_name} ${node_dir} ${manual}"
    #generate sdk cert
    execute_cmd "cd ${current_dir}"
    execute_cmd "cd ${guomi_dir}"
    execute_cmd "./gmsdk.sh ${sdk_name} ${node_dir} ${manual}" 
    execute_cmd "cd ${current_dir}"
}

if [ ${gen_guomi_cert} -eq 1 ];then
    gen_guomi_cert_func "${agency_name}" "${agency_dir}" "${node_name}" "${output_dir}"
    exit 0
fi

if [ ! -d "$agency_dir" ]; then
    help "$agency_dir DIR don't exist! please Check DIR!"
elif [ ! -f "$agency_dir/agency.key" ]; then
    help "$agency_dir/agency.key don't exist! please Check the file!"
elif [ ! -d "$output_dir" ]; then
    help "Error: \"$output_dir\" is not exist"
elif [ -f "$output_dir/node.key" ]; then
    echo "Attention! Duplicate generation of \"$output_dir\". Overwrite?"
    yes_go_other_exit
fi
cd  $agency_dir
node=tmp_node`date +%s`
mkdir -p $node

generate_cert_config ./

openssl ecparam -out node.param -name secp256k1
openssl genpkey -paramfile node.param -out node.key
openssl pkey -in node.key -pubout -out node.pubkey

if [ -z $manual_input ]; then 
    openssl req -new -key node.key -config cert.cnf -out node.csr -batch
else
    openssl req -new -key node.key -out node.csr
fi

openssl x509 -req -days 3650 -in node.csr -CAkey agency.key -CA agency.crt -force_pubkey node.pubkey -out node.crt -CAcreateserial -extensions v3_req -extfile cert.cnf
openssl ec -in node.key -outform DER |tail -c +8 | head -c 32 | xxd -p -c 32 | cat >node.private
openssl ec -in node.key -text -noout | sed -n '7,11p' | sed 's/://g' | tr "\n" " " | sed 's/ //g' | awk '{print substr($0,3);}'  | cat >node.nodeid

if [ "" != "`openssl version | grep 1.0.2k`" ];
then
    openssl x509  -text -in node.crt | sed -n '5p' |  sed 's/://g' | tr "\n" " " | sed 's/ //g' | sed 's/[a-z]/\u&/g' | cat >node.serial
else
    openssl x509  -text -in node.crt | sed -n '4p' | sed 's/ //g' | sed 's/.*(0x//g' | sed 's/)//g' |sed 's/[a-z]/\u&/g' | cat >node.serial
fi

cp ca.crt agency.crt $node
mv node.key node.csr node.crt node.param node.private node.pubkey node.nodeid node.serial  $node

nodeid=`cat $node/node.nodeid | head`
serial=`cat $node/node.serial | head`

cat>$node/node.json <<EOF
 {
 "id":"$nodeid",
 "name":"$node_name",
 "agency":"$agency_name",
 "caHash":"$serial"
}
EOF

	cat>$node/node.ca <<EOF
	{
	"serial":"$serial",
	"pubkey":"$nodeid",
	"name":"$node_name"
	}
EOF
cd -
pwd
cp $agency_dir/$node/* $output_dir
rm $agency_dir/$node -rf
LOG_INFO "Success! $node_name cert files has been generated into \"`readlink -f $output_dir`\""

