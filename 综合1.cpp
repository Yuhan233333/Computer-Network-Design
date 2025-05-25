#include<iostream>
#include <fstream>
#include<vector>
#include <sstream>  // ⬅️ 用于 istringstream 和 ostringstream
int i=1;
int adminnumber=1;
int customernumber=1;
using namespace std;
class Goods {
private:
    string name;
    int ii;
    int quantity;
    double price;
    double discount; // 0.9 表示 9 折
	string owner;
public:
    Goods(string n = "", int q = 0, double p = 0.0, double d = 1.0,string o="unnameed")
        : name(n), quantity(q), price(p), discount(d),owner(o) {}

    void show() const {
        cout << name << "number: " << quantity
             << "origin price: " << price
             << "discount: " << discount
             << "price: " << getDiscountedPrice() << endl;
    }

    double getDiscountedPrice() const { return price * discount; }

    string getName() const { return name; }
    int getQuantity() const { return quantity; }

    bool reduce(int amount) {
        if (amount > quantity) return false;
        quantity -= amount;
        return true;
    }

    void saveToFile(ofstream& fout) const {
        fout << i<<" "<<name << " " << quantity << " " << price << " " << discount <<" "<<owner<< endl;
        ++i;
    }

    // 从文件中读取
    static Goods loadFromFile(ifstream& fin) {
        string n;
        int q;
        double p, d;
        fin >> n >> q >> p >> d;
        return Goods(n, q, p, d);
    }
};


class user{
	protected:
		string type;
		string name;
		string password;
		double money;
	public:
		user(string type, string name, string password,double money): type(type), name(name), password(password),money(money) {
       // cout << "User " << name << " has been created" << endl;
    }
		virtual ~user()
		{
		}
		string virtual getUserType() const=0;
};


class admin : public user{
	private:
		string type;
		string name;
		string password;
		double money;
	public:
		admin(string type,string name,string password,double money,bool isregister):user(type,name,password,money){
			int flag=0;
			vector<string> lines;
			string line;
			ifstream fin("admin.txt");
			if (isregister==false)
			{
			if (!fin) 
			{
        	cout << "opening admin.txt fail\n"<<endl;
   			}
   			else{
   				while (getline(fin, line)) {
		        lines.push_back(line);
		    }
		    fin.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string adminname,adminpwd;
			    double adminmoney;
			
			    // 尝试解析整行
			    if (iss >> id >> adminname >> adminpwd >> adminmoney&&adminname!=name) {
				}
				else {
					cout<<"sorry we have another same name admin"<<endl;
					flag=1;
				}
			}
		}
			if (flag==0)
			{
				ifstream fin2("customer.txt");
				if (!fin2) 
			{
        	cout << "opening customer.txt fail\n"<<endl;
   			}
   			else{
   				while (getline(fin2, line)) {
		        lines.push_back(line);
		    }
		    fin2.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string customername,customerpwd;
			    double customermoney;
			
			    // 尝试解析整行
			    if (iss >> id >> customername >> customerpwd >> customermoney&&customername!=name) {
				}
				else {
					cout<<"sorry we have another same name customer"<<endl;
					flag=1;
				}
			}
			}
			   }
			   if (flag==0)
   				{
   				ofstream fout("admin.txt", ios::app);
				fout<<adminnumber<<" "<<name<<" "<<password<<" "<<money<<endl;
   				cout<<"admin "<< name<<" has been created"<<endl;
   				adminnumber++;
   				}
   			}
		}
		~admin() override {
		}
		string getUserType() const override {
        return "admin";
    }
		void supply()
		{
			ofstream fout("inventory.txt", ios::app);
    		if (!fout) 
			{
        	cout << "creating inventory.txt fail\n"<<endl;
   			}
   			else
   			{
   				string goodsname;
   				int quantity;
   				double price;
   				double discount;
   				cout<<"please input the good's name , the quantity, the price and the discount.(quit is quit)"<<endl;
   				cin>>goodsname;
   				while (name!="quit")
   				{
   				cin>>quantity>>price>>discount;
   				Goods Goods1(goodsname,quantity,price,discount,name);
   				Goods1.saveToFile(fout);
   				cout<<name<<" has been supplyed "<<quantity<<" sets"<<endl;
   				cout<<"please input the good's name , the quantity, the price and the discount.(quit is quit)"<<endl;
   				cin>>goodsname;
				}
				fout.close();
			}
		}
};


class customer: public user{
	public:
		customer (string type,string name ,string password,double money,bool isregister):user(type,name,password,money){
			if (isregister==false)
			{
			int flag=0;
			vector<string> lines;
			string line;
			ifstream fin("admin.txt");
			if (!fin) 
			{
        	cout << "opening admin.txt fail\n"<<endl;
   			}
   			else{
   				while (getline(fin, line)) {
		        lines.push_back(line);
		    }
		    fin.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string adminname,adminpwd;
			    double adminmoney;
			
			    // 尝试解析整行
			    if (iss >> id >> adminname >> adminpwd >> adminmoney&&adminname!=name) {
				}
				else {
					cout<<"sorry we have another same name admin"<<endl;
					flag=1;
				}
			}
		}
			if (flag==0)
			{
				ifstream fin2("customer.txt");
				if (!fin2) 
			{
        	cout << "opening customer.txt fail\n"<<endl;
   			}
   			else{
   				while (getline(fin2, line)) {
		        lines.push_back(line);
		    }
		    fin2.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string customername,customerpwd;
			    double customermoney;
			
			    // 尝试解析整行
			    if (iss >> id >> customername >> customerpwd >> customermoney&&customername!=name) {
				}
				else {
					cout<<"sorry we have another same name customer"<<endl;
					flag=1;
				}
			}
			}
			   }
			   if (flag==0)
   				{
   				ofstream fout("customer.txt", ios::app);
				fout<<customernumber<<" "<<name<<" "<<password<<" "<<money<<endl;
   				cout<<"customer "<< name<<" has been created"<<endl;
   				customernumber++;
   				}
   			}
		}
		~customer() override
		{	
		}
		
		
		void recharge(double m,string name)
		{
			cout<<"please input the number of your want to recharge"<<endl;
			money=money+m;
			cout<<"recharge success"<<endl;
		}
		void purchase() {
			int id, quantity;
		    string name;
		    int ii;
		    double price, discount;
		    int target, buy_amount;
		    vector<string> lines;
		    ifstream fin("inventory.txt");
		    string line;
		
		    while (getline(fin, line)) {
		        lines.push_back(line);
		    }
		    fin.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id, quantity;
			    string name;
			    double price, discount;
			
			    // 尝试解析整行
			    if (iss >> id >> name >> quantity >> price >> discount) {
				    if (quantity > 0) 
				    ii++;
				}

			}
			cout << "we have " << ii << " goods"<<endl;
			ii=0;
		    // 显示全部商品（可选）
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id, quantity;
			    string name;
			    double price, discount;
				string owner_name;
			    // 尝试解析整行
			    if (iss >> id >> name >> quantity >> price >> discount>>owner_name) {
				    if (quantity > 0) {
				        cout << lines[i] << endl;
				    } else {
				         cout << "sold out " << name << endl;
				    }
				}

			}

		
		    cout << "input your target and amount(0 is quit)"<<endl;
		    cin >> target;
		    while(target!=0)
			{
				cin>>buy_amount;
				if (target < 1 || target > lines.size()) {
		        cout << "error!" << endl;
		        return;
		    }
		
		    // 从对应行提取数据
		    string temp = lines[target - 1];
		    istringstream iss(temp);
		    iss >> id >> name >> quantity >> price >> discount;
		
		    if (buy_amount > quantity) {
		    	if (quantity!=0)
		        cout << "fail! we only have " << quantity << " this goods"<<endl;
		        else
		        {
		        	cout << "all this goods has been sold out "<<endl;
				}
		        continue;
		    }
		
		    quantity -= buy_amount;
		    ostringstream oss;
		    oss << id << " " << name << " " << quantity << " " << price << " " << discount;
		
		    lines[target - 1] = oss.str();
		
		    ofstream fout("inventory.txt");
		    for (const string& l : lines) {
		        fout << l << endl;
		    }
		    fout.close();
		
		    double total = price * discount * buy_amount;
		    money -= total;
		    cout << "success! cost " << total << "yuan now you have " << money <<" yuan"<< endl;
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id, quantity;
			    string name;
			    double price, discount;
			
			    // 尝试解析整行
			    if (iss >> id >> name >> quantity >> price >> discount) {
				    if (quantity > 0) 
				    ii++;
				}

			}
			cout << " we have " << ii << " goods"<<endl;
			ii=0;
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id, quantity;
			    string name;
			    double price, discount;
			
			    // 尝试解析整行
			    if (iss >> id >> name >> quantity >> price >> discount) {
				    if (quantity > 0) {
				        cout << lines[i] << endl;
				    } else {
				         cout << "sold out " << name << endl;
				    }
				}

			}
		    cout << "input your target and amount(0 is quit)"<<endl;
		    cin>>target;
			}
    
}

		string getUserType() const override {
    return "customer";
}

};
int main()
{
	customer a2("customer", "", "", 0, true);
	admin a1("admin","","",0,true);
	cout<<"clean all files?"<<endl;
	string d;
	cin>>d;
	if (d=="Y")
	{
	ofstream fout("inventory.txt");
	fout.close();
	ofstream fout2("admin.txt");
	fout2.close();
	ofstream fout3("customer.txt");
	fout3.close();
	cout<<"clean successs"<<endl;
	}
	string name,password;
	double money;
	cout<<"choose the service 1 register 2 logon 0 quit"<<endl;
	int choice;
	cin>> choice;
	while (choice!=0)
	{
	if (choice==1)
	{
		cout<<"choose the user's Identity: 1 is admin, 2 is customer"<<endl;
		int choice2;
		cin>>choice2;
		if (choice2==1)
		{
			cout<<"please input the name and password"<<endl;
			string adminname,adminpwd;
			cin>>adminname>>adminpwd;
			admin a3("admin",adminname,adminpwd,0,false);
			a3.~admin();
		}
		if (choice2==2)
		{
			cout<<"please input the name and password"<<endl;
			string customername,customerpwd;
			cin>>customername>>customerpwd;
			customer a4("customer",customername,customerpwd,0,false);
			a4.~customer();
		}
	}
	if (choice==2)
	{
		int flag=0;
		vector<string> lines;
		string line;
		cout<<"input your name and password"<<endl;
		string name,password;
		cin>>name>>password;
		ifstream fin("customer.txt");
		lines.clear(); 
		while (getline(fin, line)) {
		        lines.push_back(line);
		    }
		    fin.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string customername,customerpwd;
			    double customermoney;
			    if (iss >> id >> customername >> customerpwd >> customermoney&&customername==name&&customerpwd==password) {
			    	cout<<"customer "<<name<<" log in success"<<endl;
			    	customer a2("customer",name,password,customermoney,true);
			    	flag=1;
				}
			}
		ifstream fin2("admin.txt");
		lines.clear(); 
		while (getline(fin2, line)) {
		        lines.push_back(line);
		    }
		    fin2.close();
		    for (size_t i = 0; i < lines.size(); i++) {
			    istringstream iss(lines[i]);
			    int id;
			    string adminname,adminpwd;
			    double adminmoney;
			    if (iss >> id >> adminname >> adminpwd >> adminmoney&&adminname==name&&adminpwd==password) {
			    	cout<<"admin "<<name<<" log in success"<<endl;
			    	admin a1("admin",name,password,adminmoney,true);
			    	flag=2;
				}
			}
			if (flag==0)
			{
				cout<<"wrong user name or wrong password"<<endl;
			}
			if (flag==1)
			{
				cout<<"please choose your service(1 recharge, 2 buy, 0 quit)"<<endl;
				int customer_choice;
				cin>> customer_choice;
				while (customer_choice!=0)
				{
				if (customer_choice==1)
				{
					double money;
					cin>>money;
					a2.recharge(money,name);
				}
				if (customer_choice==2)
				{
					a2.purchase();
				}
				}
				cout<<"Thanks for your using"<<endl;
			}
	}
	cout<<"1 register 2 logon 0 quit"<<endl;
	cin>> choice;
   }
   cout<<"quit success"<<endl;
}