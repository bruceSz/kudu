
namespace kudu {
namespace kudu_manager {
class ServiceIf {
  public:
    virtual Status add() = 0;
};


class KuduServiceIf{
  public:
  virtual add() override;
  ~KuduServiceIf();
  static Status CreateKuduServiceIf(std::string kudu_master_addr, std::shared_ptr<KuduServiceIf>* ksi);
  private:
    KuduServiceIf();

    KuduServiceIf& operator=(const KuduServiceIf& other) = delete;
    KuduServiceIf(const KuduServiceIf& other) = delete;

};
} // kudu_manager
} // kudu
