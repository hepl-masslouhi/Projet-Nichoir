from sqlalchemy.orm import DeclarativeBase, mapped_column, relationship
from sqlalchemy import Integer,String, create_engine, ForeignKey,text

class Base(DeclarativeBase):
    pass

class Images(Base):
    __tablename__ = "Images"
    id = mapped_column(Integer, primary_key = True) 
    image_path = mapped_column(String(255))
    date = mapped_column(String(20))

    def __str__(self):
        return f"{self.id} : {self.image_path}  Date : {self.date}"


class Tensions(Base):
    __tablename__ = "Tensions"
    id = mapped_column(Integer, primary_key = True)
    tension = mapped_column(String(20), nullable = False)
    date = mapped_column(String(20))


    def __str__(self):
        return f"{self.id} : Tension = {self.tension}, Date = {self.date}"

def main():
    engine = create_engine("mysql+pymysql://pious:pious@172.20.10.4/mqtt", echo = True)
    Base.metadata.create_all(engine)

if __name__ == "__main__":
    main()
